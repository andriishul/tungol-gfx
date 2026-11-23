#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <functional>
#include <memory>
#include <string>

namespace tungol::runtime
{
	struct TextureDesc 
	{
		vhl::Format fmt;

		uint32_t width = 1;
		uint32_t height = 1;

		uint32_t mipLevels = 1;
	};

	struct Texture 
	{
		std::unique_ptr<vhl::Image> image;
		std::unique_ptr<vhl::ImageView> view;
		std::unique_ptr<vhl::Sampler> sampler;
	};


	TUNGOL_EXPORT Texture createTexture(const vhl::Device& device, vhl::SegregatedAllocator& allocator, const TextureDesc& desc);

	class TUNGOL_EXPORT TextureList 
	{
	public:
		TextureList(RenderKernel& kernel, MaterialHeap& matHeap, uint32_t maxTextures);
		~TextureList();

		void foreachImage(const std::function<void(vhl::Image&)>& fn) const;

		uint32_t add(Texture&& texture);
		void remove(uint32_t idx);

		void retain(uint32_t frameSlot);
		bool release(uint32_t frameSlot);

		const vhl::DescriptorSetHandle& set() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
