#include "FormatSupport.hpp"

#include "Device.hpp"

#include <vhl/utils/VkUtils.hpp>

namespace tungol::vhl 
{

	bool FormatSupport::is_supported(const Device& device, Format format, ImageType imageType, ImageUsageFlags usage, ImageFormatDesc& desc)
	{
        VkImageFormatProperties props{};

        VkPhysicalDevice physical = nullptr;
        VkFormat vkFormat = utils::toVkFormat(format);
        VkImageUsageFlags vkUsage = utils::toVkImageUsageFlags(usage);

        if (uint64_t handle_value = device.adapterHandle().value; handle_value != 0)
            physical = reinterpret_cast<VkPhysicalDevice>(handle_value);
        else throw std::runtime_error("[FormatSupport::is_supported] Null adapter handle");

        VkResult result = vkGetPhysicalDeviceImageFormatProperties(physical, vkFormat,
            utils::toVkImageType(imageType), VK_IMAGE_TILING_OPTIMAL, vkUsage, 0, &props);

        if (result == VK_SUCCESS)
        {
            desc.maxArrayLayers = props.maxArrayLayers;
            desc.maxMipLevels = props.maxMipLevels;
            desc.samples = utils::toSampleCount(props.sampleCounts);
            desc.maxHeight = props.maxExtent.height;
            desc.maxWidth = props.maxExtent.width;
        }

		return result == VK_SUCCESS;
	}
}
