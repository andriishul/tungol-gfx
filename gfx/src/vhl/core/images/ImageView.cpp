#include "ImageView.hpp"
#include <vhl/utils/VkUtils.hpp>

namespace tungol::vhl {

	struct ImageView::Impl 
	{
		VkImageView view = VK_NULL_HANDLE;
		VkDevice logical = VK_NULL_HANDLE;

		uint32_t baseMipLevel = 0, baseArrayLayer = 0;

		ImageAspectFlags aspect = ImageAspectFlags::Color;

		uint32_t levelCount = 1, layerCount = 1;

		Format format = Format::Undefined;

		ImageViewType viewType = ImageViewType::TwoD;
	};

	ImageView::ImageView(const Device& device, const Image& image, const ImageViewDesc& desc) : m_impl(new Impl)
	{
		VkImage vkImage = VK_NULL_HANDLE;
		
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[ImageView::ImageView] Null device handle");

		if (uint64_t handle_value = image.handle().value; handle_value != 0)
			vkImage = reinterpret_cast<VkImage>(handle_value);
		else throw std::runtime_error("[ImageView::ImageView] Null image handle");

		VkImageSubresourceRange subresource
		{
			.aspectMask = utils::toVkImageAspectFlags(desc.aspect),
			.baseMipLevel = desc.baseMipLevel,
			.levelCount = desc.levelCount == RemainingMipLevels ? VK_REMAINING_MIP_LEVELS : desc.levelCount,
			.baseArrayLayer = desc.baseArrayLayer,
			.layerCount = desc.layerCount == RemainingArrayLayers ? VK_REMAINING_ARRAY_LAYERS : desc.layerCount
		};


		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vkImage,
			.viewType = utils::toVkImageViewType(desc.viewType),
			.format = utils::toVkFormat(desc.format),
			.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange = subresource
		};


		VK_CHECK(vkCreateImageView(m_impl->logical, &viewInfo, nullptr, &m_impl->view));
	}

	ImageView::~ImageView()
	{
		if (m_impl && m_impl->logical && m_impl->view) 
		{
			vkDestroyImageView(m_impl->logical, m_impl->view, nullptr);
		}
	}

	uint32_t ImageView::baseMipLevel() const
	{
		return m_impl->baseMipLevel;
	}

	uint32_t ImageView::levelCount() const
	{
		return m_impl->levelCount;
	}

	uint32_t ImageView::baseArrayLayer() const
	{
		return m_impl->baseArrayLayer;
	}

	uint32_t ImageView::layerCount() const
	{
		return m_impl->layerCount;
	}

	Format ImageView::format() const
	{
		return m_impl->format;
	}

	ImageAspectFlags ImageView::aspect() const
	{
		return m_impl->aspect;
	}
	ImageViewType ImageView::type() const
	{
		return m_impl->viewType;
	}
	ImageViewHandle ImageView::handle() const
	{
		return ImageViewHandle{ reinterpret_cast<uint64_t>(m_impl->view) };
	}
}
