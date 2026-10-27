#pragma once
#include "Image.hpp"

namespace tungol::vhl 
{
	struct ImageViewDesc 
	{
		Format format;

		ImageViewType viewType = ImageViewType::TwoD;

		ImageAspectFlags aspect = ImageAspectFlags::Color;

		uint32_t baseMipLevel = 0;
		uint32_t levelCount = RemainingMipLevels;          
		uint32_t baseArrayLayer = 0;
		uint32_t layerCount = RemainingArrayLayers;
	};
	class TUNGOL_EXPORT ImageView 
	{
	public:
		ImageView(const Device& device, const Image& image, const ImageViewDesc& desc);
		~ImageView();

		uint32_t baseMipLevel() const;
		uint32_t levelCount() const;
		uint32_t baseArrayLayer() const;
		uint32_t layerCount() const;
		Format format() const;
		ImageAspectFlags aspect() const;
		ImageViewType type() const;

		ImageViewHandle handle() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
