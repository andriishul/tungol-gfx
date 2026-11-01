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


	TUNGOL_EXPORT Texture loadTexture(vhl::Device& device, vhl::SegregatedAllocator& allocator, vhl::CopyContext& cpy, const std::string& path);

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
