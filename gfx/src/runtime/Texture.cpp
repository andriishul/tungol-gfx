#include "Texture.hpp"

#include "helper/CatchGraphicsError.hpp"

#include <vhl/core/Device.hpp>
#include <vhl/core/Allocator.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/Descriptors.hpp>
#include <vhl/commands/CopyContext.hpp>

#include <asset/BcTextureReader.hpp>

#include <sstream>
#include <stdexcept>
#include <chrono>
namespace tungol::runtime
{
	Texture loadTexture(vhl::Device& device, vhl::SegregatedAllocator& allocator, vhl::CopyContext& cpy, const std::string& path)
	{
        auto bcTexture = asset::load_bc_texture(path);
               
        const vhl::Format fmt = static_cast<vhl::Format>(bcTexture.header.format);

        vhl::ImageDesc imageDesc
        {
            .format = fmt,
            .width = bcTexture.header.width,
            .height = bcTexture.header.height,
            
            .usage = vhl::ImageUsageFlags::Sampled | vhl::ImageUsageFlags::TransferDst,

            .levelCount = bcTexture.header.mipLevels
        };

        auto image = std::make_unique<vhl::Image>(device, allocator, imageDesc);

        cpy.upload(*image, bcTexture.mips[0].data(), bcTexture.mips[0].size(), 0);

        for (uint32_t level = 1; level < bcTexture.mips.size(); ++level)
        {
            cpy.upload(*image, bcTexture.mips[level].data(), bcTexture.mips[level].size(), level);
        }


        auto view = std::make_unique<vhl::ImageView>(device, *image, vhl::ImageViewDesc{ fmt });
        
        auto sampler = std::make_unique<vhl::Sampler>(device);

        return Texture{ std::move(image), std::move(view), std::move(sampler)};
	}

    struct TextureRegistry::Impl 
    {
        vhl::Device& device;
        vhl::DescriptorPool& descriptorPool;

        uint32_t nextSlot = 0;
        std::vector<Texture> textures;
        uint32_t maxTextures = 0;

        vhl::DescriptorSetHandle texturesSet;
    };


    TextureRegistry::TextureRegistry(vhl::Device& dev, vhl::DescriptorPool& pool, vhl::DescriptorSetLayoutList& layouts, uint32_t maxTextures) : m_impl(new Impl{ dev, pool })
    {
        m_impl->texturesSet = pool.emplace(layouts[2]);
        m_impl->maxTextures = maxTextures;

        m_impl->textures.reserve(maxTextures);
    }
    TextureRegistry::~TextureRegistry() 
    {
        try
        {
            m_impl->device.wait();
        }
        CATCH_GRAPHICS_ERROR

        m_impl->descriptorPool.remove(m_impl->texturesSet);
        m_impl->textures.clear();
    }

    void TextureRegistry::foreachImage(const std::function<void(vhl::Image&)>& fn) const
    {
        for (auto& tex : m_impl->textures) 
        {
            if(tex.image) fn(*tex.image);
        }
    }

    uint32_t TextureRegistry::add(Texture&& texture)
    {
        if (m_impl->nextSlot >= m_impl->maxTextures)
        {
            std::ostringstream ss;
            ss << "[TextureRegistry::add] bindless capacity exceeded ("
                << m_impl->nextSlot << " / " << m_impl->maxTextures<< "). Raise k_MaxBindlessTexturesPerScene if you want.";
            throw std::runtime_error(ss.str());
        }

        uint32_t slot = m_impl->nextSlot++;

        m_impl->textures.push_back(std::move(texture));

        vhl::DescriptorWriter(m_impl->texturesSet)
            .sampledImage(*m_impl->textures.back().view, *m_impl->textures.back().sampler, 0, vhl::ImageLayout::ShaderReadOnly, slot)
            .apply(m_impl->device);

        return slot;
    }

    const vhl::DescriptorSetHandle& TextureRegistry::set()
    {
        return m_impl->texturesSet;
    }
}
