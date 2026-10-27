#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <functional>
#include <memory>
#include <string>

namespace tungol::runtime
{
	struct Texture 
	{
		std::unique_ptr<vhl::Image> image;
		std::unique_ptr<vhl::ImageView> view;
		std::unique_ptr<vhl::Sampler> sampler;
	};


	// Loads a texture from disk into GPU memory.
	// NOTE: mipmaps generated on CPU.
	// Set <channels> to 4 for RGBA (Albedo/Normal), 2 for RG (Metallic-Roughness), etc.
	TUNGOL_EXPORT Texture loadTexture(vhl::Device& device, vhl::SegregatedAllocator& allocator, vhl::CopyContext& cpy, const std::string& path, vhl::Format format, int channels, bool srgb);

	class TUNGOL_EXPORT TextureRegistry 
	{
	public:
		TextureRegistry(vhl::Device& dev, vhl::DescriptorPool& pool, vhl::DescriptorSetLayoutList& layouts, uint32_t maxTextures);
		~TextureRegistry();

		void foreachImage(const std::function<void(vhl::Image&)>& fn) const;

		uint32_t add(Texture&& texture);

		const vhl::DescriptorSetHandle& set();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
