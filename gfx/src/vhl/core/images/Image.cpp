#include "Image.hpp"

#include <vhl/core/FormatSupport.hpp>

#include <vhl/utils/VkUtils.hpp>

namespace tungol::vhl 
{
	struct Image::Impl
	{
		SegregatedAllocator* allocator = nullptr;
		const Device* device = nullptr;

		bool ownsHandle = true;

		image::ImageResource resource{};
		
		ImageHandle handle{};

		ImageLimits limits{};

		SampleCount samples = SampleCount::One;
		uint32_t height = 1, width = 1;
		uint32_t depth = 1;
		Format format = Format::Undefined;

		ImageType type = ImageType::TwoD;

		ImageUsageFlags usage = ImageUsageFlags::Color;

		uint32_t levelCount = 1, layerCount = 1;
	};

	Image::Image(Image&& img) noexcept : m_impl(std::move(img.m_impl))
	{}

	Image& Image::operator=(Image&& img) noexcept
	{
		if (this != &img)
			m_impl = std::move(img.m_impl);
		return *this;
	}

	Image::Image(const Device& device, SegregatedAllocator& allocator, const ImageDesc& imageDesc) : m_impl(new Impl)
	{
		ImageFormatDesc formatDesc{};

		if (!FormatSupport::is_supported(device, imageDesc.format, imageDesc.imageType, imageDesc.usage, formatDesc))
		{
			std::ostringstream os;
			os << "[Image::Image] Format " << utils::FormatToString(imageDesc.format)
				<< " with usage " << utils::ImageUsageToString(imageDesc.usage)
				<< " is not supported by this GPU.\n";
			throw std::runtime_error(os.str());
		}

		std::ostringstream os;
		bool hasError = false;

		if (formatDesc.maxWidth < imageDesc.width)
		{
			os << "Width " << imageDesc.width
				<< " exceeds hardware limit of " << formatDesc.maxWidth << ".\n";
			hasError = true;
		}

		if (formatDesc.maxHeight < imageDesc.height)
		{
			os << "Height " << imageDesc.height
				<< " exceeds hardware limit of " << formatDesc.maxHeight << ".\n";
			hasError = true;
		}

		if (formatDesc.maxMipLevels < imageDesc.levelCount)
		{
			os << "Mip count " << imageDesc.levelCount
				<< " exceeds hardware limit of " << formatDesc.maxMipLevels << ".\n";
			hasError = true;
		}

		if (formatDesc.maxArrayLayers < imageDesc.layerCount)
		{
			os << "Array layers " << imageDesc.layerCount
				<< " exceeds hardware limit of " << formatDesc.maxArrayLayers << ".\n";
			hasError = true;
		}

		if (!samplesSupported(formatDesc.samples, imageDesc.samples))
		{
			os << "MSAA level " << static_cast<uint32_t>(imageDesc.samples)
				<< " is not supported. Hardware supports: " << static_cast<uint32_t>(formatDesc.samples) << ".\n";
			hasError = true;
		}

		if (hasError)
		{
			throw std::runtime_error("[Image::Image] " + os.str());
		}
		m_impl->allocator = &allocator;
		m_impl->device = &device;

		m_impl->resource = image::create(device, allocator, imageDesc);
		m_impl->handle = m_impl->resource.handle;

		m_impl->ownsHandle = true;
		m_impl->type = imageDesc.imageType;
		m_impl->format = imageDesc.format;
		m_impl->height = imageDesc.height;
		m_impl->width = imageDesc.width;
		m_impl->depth = imageDesc.depth;
		m_impl->usage = imageDesc.usage;
		m_impl->layerCount = imageDesc.layerCount;
		m_impl->levelCount = imageDesc.levelCount;
		m_impl->samples = imageDesc.samples;
		m_impl->limits = device.limits().image;
	}

	// Done to wrap swapchain images in a more presentable form, not as raw bunch pointers. Anyway, the swapchain deletes them internally.
	Image::Image(const ImageSource& source) : m_impl(new Impl)
	{
		m_impl->format = source.format;

		m_impl->width = source.width;
		m_impl->height = source.height;

		m_impl->ownsHandle = false;
		m_impl->handle = source.handle;
	}


	Image::~Image()
	{
		if (dispose) dispose(this);

		if (m_impl && m_impl->ownsHandle && m_impl->device && m_impl->allocator) 
		{
			image::destroy(*m_impl->device, *m_impl->allocator, m_impl->resource);
		}
	}


	ImageUsageFlags Image::usage() const
	{
		return m_impl->usage;
	}

	uint32_t Image::levelCount() const
	{
		return m_impl->levelCount;
	}

	uint32_t Image::layerCount() const
	{
		return m_impl->layerCount;
	}

	uint32_t Image::width() const
	{
		return m_impl->width;
	}
	uint32_t Image::height() const
	{
		return m_impl->height;
	}

	uint32_t Image::depth() const
	{
		return m_impl->depth;
	}

	Format Image::format() const
	{
		return m_impl->format;
	}
	SampleCount Image::samples() const
	{
		return m_impl->samples;
	}
	ImageType Image::type() const
	{
		return m_impl->type;
	}
	ImageHandle Image::handle() const
	{
		return m_impl->handle;
	}

	ImageLimits Image::limits() const
	{
		return m_impl->limits;
	}

	ImageAspectFlags aspectOf(const Image& img)
	{
		const Format format = img.format();
		const ImageUsageFlags usage = img.usage();

		if ((usage & ImageUsageFlags::DepthStencil) != ImageUsageFlags::None)
		{
			switch (format)
			{
				case Format::D16Unorm:
				case Format::D32Float:
					return ImageAspectFlags::Depth;

				case Format::D24UnormS8Uint:
				case Format::D32FloatS8Uint:
					return ImageAspectFlags::Depth | ImageAspectFlags::Stencil;

				default: return ImageAspectFlags::Depth;
			}
		}

		return ImageAspectFlags::Color;
	}
}
