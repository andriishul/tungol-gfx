#include "Texture.hpp"

#include "helper/CatchGraphicsError.hpp"

#include <vhl/core/Device.hpp>
#include <vhl/core/Allocator.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/Descriptors.hpp>
#include <vhl/commands/CopyContext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <thirdparty/stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <thirdparty/stb/stb_image_resize2.h>

#include <sstream>
#include <stdexcept>
#include <chrono>
namespace tungol::runtime
{
	Texture loadTexture(vhl::Device& device, vhl::SegregatedAllocator& allocator, vhl::CopyContext& cpy, const std::string& path, vhl::Format format, int channels, bool srgb)
	{
        int w, h, src;

       
        stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &src, channels);
       
        if (!pixels) throw std::runtime_error("[Texture loadTexture()] failed: " + path);

        uint32_t mipCount = 1;
        for (uint32_t s = std::max(w, h); s > 1; s >>= 1) mipCount++;

        vhl::ImageDesc imageDesc
        {
            .format = format,
            .width = static_cast<uint32_t>(w),
            .height = static_cast<uint32_t>(h),
            
            .usage = vhl::ImageUsageFlags::Sampled | vhl::ImageUsageFlags::TransferDst,

            .levelCount = mipCount
        };

        auto image = std::make_unique<vhl::Image>(device, allocator, imageDesc);

        cpy.upload(*image, pixels, static_cast<uint64_t>(w * h * channels), 0);

        std::vector<uint8_t> prev(pixels, pixels + w * h * channels);
        std::vector<uint8_t> next;
        uint32_t pw = w, ph = h;
       
        stbir_pixel_layout layout = (channels == 4) ? STBIR_RGBA : (channels == 3) ? STBIR_RGB :
            (channels == 2) ? STBIR_2CHANNEL : STBIR_1CHANNEL;

        for (uint32_t level = 1; level < mipCount; ++level)
        {
            uint32_t nw = std::max<uint32_t>(1, pw / 2);
            uint32_t nh = std::max<uint32_t>(1, ph / 2);
            next.resize(nw * nh * channels);

            if (srgb) stbir_resize_uint8_srgb(prev.data(), pw, ph, 0, next.data(), nw, nh, 0, layout);
            else stbir_resize_uint8_linear(prev.data(), pw, ph, 0, next.data(), nw, nh, 0, layout);


            cpy.upload(*image, next.data(), next.size(), level);

            prev.swap(next);
            pw = nw;
            ph = nh;
        }


        auto view = std::make_unique<vhl::ImageView>(device, *image, vhl::ImageViewDesc{ format });
        
        auto sampler = std::make_unique<vhl::Sampler>(device);

        stbi_image_free(pixels);


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
