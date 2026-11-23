#include "Texture.hpp"

#include "RenderKernel.hpp"
#include "internal/KernelInternal.hpp"

#include <vhl/core/Device.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/GraphicsContext.hpp>

#include <runtime/heaps/MaterialHeap.hpp>

#include <vhl/core/buffers/Buffer.hpp>
#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/Descriptors.hpp>
#include <vhl/graph/HazardTracker.hpp>

#include <sstream>
#include <stdexcept>

namespace tungol::runtime
{
	Texture createTexture(const vhl::Device& device, vhl::SegregatedAllocator& allocator, const TextureDesc& desc)
	{ 
        vhl::ImageDesc imageDesc
        {
            .format = desc.fmt,
            .width = desc.width,
            .height = desc.height,
            
            .usage = vhl::ImageUsageFlags::Sampled | vhl::ImageUsageFlags::TransferDst,

            .levelCount = desc.mipLevels
        };

        auto image = std::make_unique<vhl::Image>(device, allocator, imageDesc);

        auto view = std::make_unique<vhl::ImageView>(device, *image, vhl::ImageViewDesc{ desc.fmt });
        
        auto sampler = std::make_unique<vhl::Sampler>(device);

        return Texture{ std::move(image), std::move(view), std::move(sampler)};
	}

    struct TextureList::Impl
    {
        Impl(RenderKernel& k, MaterialHeap& mH, uint32_t mT);
        ~Impl();


        vhl::HazardTracker& tracker;
        MaterialHeap& matHeap;

        uint32_t nextSlot = 0;
        uint32_t maxTextures = 0;

        std::vector<Texture> textures;
        std::vector<uint32_t> freeSlots;
        std::vector<uint32_t> refCounts;

        vhl::Device& device;

        vhl::DescriptorPool& descPool;
        vhl::DescriptorSetLayoutList& layouts;

        vhl::SegregatedAllocator& allocator;

        vhl::DescriptorSetHandle sharedSet;
    };

    TextureList::Impl::Impl(RenderKernel& k, MaterialHeap& mH, uint32_t mT) :
        matHeap(mH), device(KernelInternal::context(k).activeDevice()), descPool(KernelInternal::descriptorPool(k)),
        allocator(KernelInternal::allocator(k)), layouts(KernelInternal::layouts(k)), tracker(KernelInternal::tracker(k)),
        maxTextures(mT)
    {
        sharedSet = descPool.emplace(layouts[2]);
        textures.reserve(maxTextures);

        auto& matBuf = matHeap.pool().buffer();
        vhl::DescriptorWriter(sharedSet).storageBuffer(matBuf, 0, 0, matBuf.size())
            .apply(device);
    }

    TextureList::Impl::~Impl() 
    {
        descPool.remove(sharedSet);
        textures.clear();
    }



    TextureList::~TextureList() = default;

    TextureList::TextureList(RenderKernel& kern, MaterialHeap& matHeap, uint32_t maxTextures) : 
        m_impl(new Impl(kern, matHeap, maxTextures))
    {
       
    }
   

    void TextureList::foreachImage(const std::function<void(vhl::Image&)>& fn) const
    {
        for (auto& tex : m_impl->textures) 
        {
            if(tex.image) fn(*tex.image);
        }
    }

    uint32_t TextureList::add(Texture&& texture)
    {
        uint32_t slot;
        if (!m_impl->freeSlots.empty())
        {
            slot = m_impl->freeSlots.back();
            m_impl->freeSlots.pop_back();
            m_impl->textures[slot] = std::move(texture);
            m_impl->refCounts[slot] = 0;
        }
        else
        {
            if (m_impl->nextSlot >= m_impl->maxTextures)
                throw std::runtime_error("[TextureList::add] bindless capacity exceeded");

            slot = m_impl->nextSlot++;
            m_impl->textures.push_back(std::move(texture));
            m_impl->refCounts.push_back(0);
        }

        auto& tex = m_impl->textures[slot];

        if (tex.image)
        {
            m_impl->tracker.emplace(*tex.image);

            vhl::DescriptorWriter(m_impl->sharedSet)
                .sampledImage(*tex.view, *tex.sampler, 1, vhl::ImageLayout::ShaderReadOnly, slot)
                .apply(m_impl->device);

            return slot;
        }
        else throw std::runtime_error("[TextureList::add] empty Image");
    }

    void TextureList::remove(uint32_t idx)
    {
        if (idx >= m_impl->textures.size()) return;
        if (!m_impl->textures[idx].image) return;

        m_impl->textures[idx] = Texture{};
        m_impl->freeSlots.push_back(idx);
    }

    void TextureList::retain(uint32_t slot)
    {
        if (slot < m_impl->refCounts.size())
            ++m_impl->refCounts[slot];
    }

    bool TextureList::release(uint32_t slot)
    {
        if (slot >= m_impl->refCounts.size()) return false;
        if (m_impl->refCounts[slot] == 0) return false;
        if (--m_impl->refCounts[slot] != 0) return false;

        if (slot < m_impl->textures.size() && m_impl->textures[slot].image)
        {
            m_impl->textures[slot] = Texture{};
            m_impl->freeSlots.push_back(slot);
        }
        return true;
    }

    const vhl::DescriptorSetHandle& TextureList::set() const
    {
        return m_impl->sharedSet;
    }
    
}
