#include "Swapchain.hpp"

#include "Device.hpp"
#include <vhl/core/images/Image.hpp>

#include "vhl/sync/Fence.hpp"
#include "vhl/sync/Semaphore.hpp"

#include "vhl/utils/VkInspector.hpp"


namespace tungol::vhl
{
	// Selects the preferred surface format: SRGB (BGRA/RGBA) if available, otherwise UNORM, otherwise the first supported format.
	static VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		for (const auto& f : formats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_SRGB ||
				f.format == VK_FORMAT_R8G8B8A8_SRGB)
			{
				return f;
			}
		}

		for (const auto& f : formats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM ||
				f.format == VK_FORMAT_R8G8B8A8_UNORM)
			{
				return f;
			}
		}

		return formats.front();
	}
	// Selects the preferred presentation mode: MAILBOX if available, otherwise the first supported mode.
	static VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& modes)
	{
		if (auto it = std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR); it != modes.cend()) 
		{
			return *it;
		}

		return modes.front();
	}
	
	struct Swapchain::Impl
	{
		VkDevice logical = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain = VK_NULL_HANDLE;

		uint32_t width = 0, height = 0;
		Format colorFormat = Format::Undefined;

		uint32_t colorImageCount = 0;

		std::vector<Image> colorImages;
	};

	Swapchain::Swapchain(const Device& device, const SurfaceDesc& desc, const Swapchain* prevSwapchain)
		: m_impl(new Impl)
	{
		VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE;

		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[Swapchain::Swapchain] Null device handle");


		if (prevSwapchain)
		{
			if (uint64_t handle_value = prevSwapchain->handle().value; handle_value != 0)
				oldSwapchain = reinterpret_cast<VkSwapchainKHR>(handle_value);
			else throw std::runtime_error("[Swapchain::Swapchain] Null device handle");
		}

		VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(desc.formats);
		VkPresentModeKHR presentMode = chooseBestPresentationMode(desc.presentationModes);


		VkSwapchainCreateInfoKHR swapchainInfo
		{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = desc.surface,
			.minImageCount = std::min(desc.capabilities.minImageCount + 1, desc.capabilities.maxImageCount),

			.imageFormat = surfaceFormat.format,
			.imageColorSpace = surfaceFormat.colorSpace,

			.imageExtent = desc.capabilities.currentExtent,
			.imageArrayLayers = 1,

			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,

			.preTransform = desc.capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = presentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = oldSwapchain
		};

		VK_CHECK(vkCreateSwapchainKHR(m_impl->logical, &swapchainInfo, nullptr, &m_impl->swapchain));

		uint32_t count = 0;

		VK_CHECK(vkGetSwapchainImagesKHR(m_impl->logical, m_impl->swapchain, &count, nullptr));

		if (count > 0)
		{
			std::vector<VkImage> vkColorImages(count);
			vkGetSwapchainImagesKHR(m_impl->logical, m_impl->swapchain, &count, vkColorImages.data());
			m_impl->colorImages.reserve(count);

			for (uint32_t i = 0; i < count; ++i)
			{
				const ImageSource source
				{
					.width = desc.capabilities.currentExtent.width,
					.height = desc.capabilities.currentExtent.height,
					.format = utils::toFormat(surfaceFormat.format),

					.handle = reinterpret_cast<uint64_t>(vkColorImages[i]),
				};

				m_impl->colorImages.emplace_back(source);
			}

			m_impl->height = desc.capabilities.currentExtent.height;
			m_impl->width = desc.capabilities.currentExtent.width;
			m_impl->colorFormat = utils::toFormat(surfaceFormat.format);
			m_impl->colorImageCount = count;
		}
	}
	Swapchain::~Swapchain() 
	{
		if(m_impl && m_impl->logical && m_impl->swapchain)
			vkDestroySwapchainKHR(m_impl->logical, m_impl->swapchain, nullptr);
	}

	AcquireResult Swapchain::acquire(const AcquireInfo& acquireInfo) const
	{
		VkSemaphore vkSemaphore = VK_NULL_HANDLE;
		VkFence vkFence = VK_NULL_HANDLE;

		if (acquireInfo.fence)
		{
			if (uint64_t handle_value = acquireInfo.fence->handle().value; handle_value != 0)
				vkFence = reinterpret_cast<VkFence>(handle_value);
		}

		if (acquireInfo.semaphore)
		{
			if (uint64_t handle_value = acquireInfo.semaphore->handle().value; handle_value != 0)
				vkSemaphore = reinterpret_cast<VkSemaphore>(handle_value);
		}

		AcquireResult result{};

		uint32_t index = 0;
		
		VkResult vkRes = vkAcquireNextImageKHR(m_impl->logical, m_impl->swapchain, UINT64_MAX, vkSemaphore, vkFence, &index);

		if(vkRes == VK_ERROR_OUT_OF_DATE_KHR)
		{
			result.success = false;
			return result;
		}

		VK_CHECK(vkRes);

		result.imageIndex = index;
		result.success = true;


		return result;
	}

	const Image& Swapchain::colorImage(uint32_t index) const
	{
		if ((uint32_t) m_impl->colorImages.size() <= index)
			throw std::runtime_error("Failed to retrieve swapchain image, no image at index:" + std::to_string(index) + " exist");

		return m_impl->colorImages[index];
	}

	Image& Swapchain::colorImage(uint32_t index)
	{
		if ((uint32_t) m_impl->colorImages.size() <= index)
			throw std::runtime_error("Failed to retrieve swapchain image, no image at index:" + std::to_string(index) + " exist");

		return m_impl->colorImages[index];
	}

	uint32_t Swapchain::colorImageCount() const
	{
		return m_impl->colorImageCount;
	}

	uint32_t Swapchain::width() const
	{
		return m_impl->width;
	}

	uint32_t Swapchain::height() const
	{
		return m_impl->height;
	}

	Format Swapchain::colorFormat() const 
	{
		return m_impl->colorFormat;
	}

	SwapchainHandle Swapchain::handle() const
	{
		return SwapchainHandle
		{
			.value = reinterpret_cast<uint64_t>(m_impl->swapchain)
		};
	}
}
