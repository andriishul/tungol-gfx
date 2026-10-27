#pragma once
#include <vhl/core/Allocator.hpp>

#include <vhl/shared/Trackable.hpp>

namespace tungol::vhl 
{
	struct ImageSource
	{
		uint32_t width, height;
		Format format;
		ImageHandle handle;
	};
	
	class TUNGOL_EXPORT Image : public Trackable
	{
	public:
		Image(const Device& device, SegregatedAllocator& allocator, const ImageDesc&);
		
		// For swapchain images
		Image(const ImageSource&);
		Image(Image&&) noexcept;
		Image& operator=(Image&&) noexcept;

		~Image();

		ImageUsageFlags usage() const;

		uint32_t levelCount() const;
		uint32_t layerCount() const;

		uint32_t width() const;
		uint32_t height() const;
		uint32_t depth() const;

		Format format() const;
		SampleCount samples() const;

		ImageType type() const;
		ImageHandle handle() const;
		ImageLimits limits() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	ImageAspectFlags aspectOf(const Image& img);
}
