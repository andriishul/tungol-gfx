#include "VkInspector.hpp"

#include <algorithm>

namespace tungol::vhl
{
	static std::string toStringVkDeviceType(VkPhysicalDeviceType type)
	{
		switch (type)
		{
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				return "IGPU";
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				return "GPU";
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				return "VIRTUAL GPU";
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				return "CPU";
			default:
				return "UNKNOWN";
		}
	}


	void Enumerator::PhysicalDevices::init(const VkInstance& instance)
	{
		s_physicalDevicesDesc.clear();

		uint32_t count = 0;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);

		if (count <= 0) 
		{
			throw std::runtime_error("[Enumerator::PhysicalDevices::init] No any physical devices found!");
		}
		
		std::vector<VkPhysicalDevice> physicalDevices(count);
		
		vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
		

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto& physical = physicalDevices[i];

			VkPhysicalDeviceVulkan13Properties vk13Props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES };
			VkPhysicalDeviceVulkan12Properties vk12Props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
			vk12Props.pNext = &vk13Props;
			VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			properties.pNext = &vk12Props;

			vkGetPhysicalDeviceProperties2(physical, &properties);
			
			VkPhysicalDeviceVulkan12Features vk12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			VkPhysicalDeviceVulkan13Features vk13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

			vk12Features.pNext = &vk13Features;

			VkPhysicalDeviceFeatures2 features
			{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &vk12Features,
			};

			vkGetPhysicalDeviceFeatures2(physical, &features);

			const auto& prop = properties.properties;

			vk13Features.pNext = nullptr;
			vk12Features.pNext = nullptr;
			features.pNext = nullptr;

			PhysicalDeviceDesc deviceDescription
			{
				.name = prop.deviceName,
				.physical = physical,
				.type = prop.deviceType,
				.limits = prop.limits,
				.properties = prop,
				.properties12 = vk12Props,
				.properties13 = vk13Props,
				.features = features.features,
				.features12 = vk12Features,
				.features13 = vk13Features
			};


			s_physicalDevicesDesc.push_back(deviceDescription);
		}
	}


	const PhysicalDeviceDesc& Enumerator::PhysicalDevices::get(const std::string& device_name)
	{
		if (auto it = std::find_if(s_physicalDevicesDesc.cbegin(), s_physicalDevicesDesc.cend(),
			[device_name](const PhysicalDeviceDesc& desc) { return desc.name ==  device_name; }); 
			it != s_physicalDevicesDesc.cend())
		{
			return *it;
		}
		std::ostringstream ss;
		ss << "[Enumerator::PhysicalDevices::get(string)] Physical device: ";
		ss << std::quoted(device_name) << " NOT FOUND!";

		throw std::runtime_error(ss.str());
	}

	const PhysicalDeviceDesc& Enumerator::PhysicalDevices::get(uint32_t index)
	{
		if (s_physicalDevicesDesc.size() > index)
		{
			return s_physicalDevicesDesc[index];
		}

		std::ostringstream ss;
		ss << "[Enumerator::PhysicalDevices::get(uint)] Physical device at index: " << index << " NOT FOUND!";

		throw std::runtime_error(ss.str());
	}
	const PhysicalDeviceDesc& Enumerator::PhysicalDevices::get(VkPhysicalDeviceType type)
	{
		if (auto it = std::find_if(s_physicalDevicesDesc.cbegin(),
			s_physicalDevicesDesc.cend(), [&type](const PhysicalDeviceDesc& deviceDesc)
			{
				return deviceDesc.type == type;
			});
			it != s_physicalDevicesDesc.cend())
		{
			return *it;
		}

		std::ostringstream ss;
		ss << "[Enumerator::PhysicalDevices::get(VkPhysicalDeviceType)] Physical device at device type: " << toStringVkDeviceType(type) << " NOT FOUND!";

		throw std::runtime_error(ss.str());
	}
	
	std::vector<std::string> Enumerator::Extensions::get_instance()
	{
		uint32_t count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

		if (count <= 0) 
		{
			throw std::runtime_error("[Enumerator::Extensions::get_instance] No instance extensions loaded!");
		}

		std::vector<VkExtensionProperties> extProperies(count);
		vkEnumerateInstanceExtensionProperties(nullptr, &count, extProperies.data());
		
		std::vector<std::string> extensionNames(count);

		std::transform(extProperies.cbegin(), extProperies.cend(), extensionNames.begin(), [](VkExtensionProperties prop) 
		{
			std::string ext = prop.extensionName;
			return ext;
		});

		return extensionNames;
	}

	std::vector<std::string> Enumerator::Extensions::get_device(const VkPhysicalDevice& physical)
	{
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(physical, nullptr,  &count, nullptr);

		if (count <= 0)
		{
			throw std::runtime_error("[Enumerator::Extensions::get_device] No device extensions loaded!");
		}
		std::vector<VkExtensionProperties> extProperties(count);
		vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extProperties.data());

		std::vector<std::string> extensionNames(count);
		std::transform(extProperties.cbegin(), extProperties.cend(), extensionNames.begin(), 
			[](const VkExtensionProperties& prop)
			{
				return prop.extensionName;
			});

		return extensionNames;
	}

	std::vector<std::string> Enumerator::Layers::get_instance()
	{
		uint32_t count = 0;
		vkEnumerateInstanceLayerProperties(&count, nullptr);

		if (count <= 0) 
		{
			throw std::runtime_error("[Enumerator::Extensions::get_instance] Failed to retrieve instance");
		}
		std::vector<VkLayerProperties> properties(count);
		vkEnumerateInstanceLayerProperties(&count, properties.data());
		
		std::vector<std::string> extensionNames(count);
		std::transform(properties.cbegin(), properties.cend(), extensionNames.begin(), 
			[](const VkLayerProperties& layer)
			{
				return std::string(layer.layerName);
			});

		return extensionNames;
	}

	std::vector<QueueFamilyDetails> Enumerator::QueueFamilyIndices::list(const VkPhysicalDevice& physical, const VkSurfaceKHR& surface)
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);

		if (count == 0) 
		{
			throw std::runtime_error("[Enumerator::QueueFamilyIndices::list] Failed to list queue families indices!");
		}

		std::vector<VkQueueFamilyProperties> queueProperties(count);
		
		vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queueProperties.data());
		
		std::vector<QueueFamilyDetails> indices(count);
		
		for (uint32_t i = 0; i < count; ++i)
		{
			indices[i].family = i;
			indices[i].count = queueProperties[i].queueCount;

			if (queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices[i].graphics = true;
			if (queueProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
				indices[i].compute = true;
			if (queueProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
				indices[i].transfer = true;

			VkBool32 has_present = VK_FALSE;

			vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &has_present);

			if (has_present)
				indices[i].present = true;
		}

		return indices;
	}

	SurfaceDesc Enumerator::Swapchain::get(const VkPhysicalDevice& physical, const VkSurfaceKHR& surface)
	{
		uint32_t count = 0;
		VkSurfaceCapabilitiesKHR capabilities;
		
		SurfaceDesc desc;

		desc.surface = surface;
		
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities) != VK_SUCCESS)
		{
			throw std::runtime_error("[Enumerator::Swapchain::get] Failed to retrieve surface capabilites.");
		}

		desc.capabilities = capabilities;


		vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);

		if (count == 0)
		{
			throw std::runtime_error("[Enumerator::Swapchain::get] Failed to retrieve surface presentation modes.");
		}

		desc.presentationModes.resize(count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, desc.presentationModes.data());

		vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
		
		if (count == 0) 
		{
			throw std::runtime_error("[Enumerator::Swapchain::get] Failed to retrieve surface formats.");
		}
		
		desc.formats.resize(count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, desc.formats.data());

		return desc;
	}


	VkPhysicalDeviceMemoryProperties Enumerator::MemoryFilter::get_physical_device_properties(const VkPhysicalDevice& physical)
	{
		VkPhysicalDeviceMemoryProperties prop;
		vkGetPhysicalDeviceMemoryProperties(physical, &prop);
		return prop;
	}

	MemoryRequirements Enumerator::MemoryFilter::get_image_requirements(
		const VkDevice& logical, const VkPhysicalDevice& physical, const VkImage& image, MemoryType required)
	{
		
		const VkPhysicalDeviceMemoryProperties prop = get_physical_device_properties(physical);
		
		VkMemoryDedicatedRequirements dedicatedMemoryRequirements { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
		
		VkMemoryRequirements memoryRequirements;

		vkGetImageMemoryRequirements(logical, image, &memoryRequirements);

		const auto requiredPropertyFlags = utils::toVkMemoryPropertyFlags(required);

		for (uint32_t i = 0; i < prop.memoryTypeCount; ++i)
		{
			if (!(memoryRequirements.memoryTypeBits & (1 << i))) continue;

			const auto& propertyFlags = prop.memoryTypes[i].propertyFlags;

			if ((propertyFlags & requiredPropertyFlags) == requiredPropertyFlags)
			{
				return MemoryRequirements
				{
					.alignment = memoryRequirements.alignment,
					.size = memoryRequirements.size,
					.memoryType = required,
					.memoryTypeIndex = i
				};
			}
		}

		throw std::runtime_error("[Enumerator::MemoryFilter::get_image_requirements] Memory type not found or not supported!");
	}

	MemoryRequirements Enumerator::MemoryFilter::get_buffer_requirements(
		const VkDevice& logical, const VkPhysicalDevice& physical, const VkBuffer& buffer, MemoryType required)
	{
		const VkPhysicalDeviceMemoryProperties prop = get_physical_device_properties(physical);

		VkMemoryRequirements memoryRequirements;

		vkGetBufferMemoryRequirements(logical, buffer, &memoryRequirements);

		const auto requiredPropertyFlags = utils::toVkMemoryPropertyFlags(required);

		for (uint32_t i = 0; i < prop.memoryTypeCount; ++i)
		{
			if (!(memoryRequirements.memoryTypeBits & (1 << i))) continue;

			const auto& propertyFlags = prop.memoryTypes[i].propertyFlags;

			if ((propertyFlags & requiredPropertyFlags) == requiredPropertyFlags)
			{
				return MemoryRequirements
				{
					.alignment = memoryRequirements.alignment,
					.size = memoryRequirements.size,
					.memoryType = required,
					.memoryTypeIndex = i				
				};
			}
		}

		throw std::runtime_error("[Enumerator::MemoryFilter::get_buffer_requirements] Memory type not found or not supported!");
	}

	VkShaderModule ShaderModule::create(const VkDevice& logical, const uint32_t* code, size_t size)
	{
		VkShaderModuleCreateInfo shaderModuleInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = size,
			.pCode = code
		};

		VkShaderModule module = VK_NULL_HANDLE;
		VK_CHECK(vkCreateShaderModule(logical, &shaderModuleInfo, nullptr, &module));

		return module;
	}

}
