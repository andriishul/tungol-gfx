#include "Device.hpp"

#include <vhl/utils/VkInspector.hpp>


namespace tungol::vhl 
{
	static void chooseDefaultFeatures(
		const DeviceDesc& desc,
		VkPhysicalDeviceFeatures& featuresCore,
		VkPhysicalDeviceVulkan12Features& features12,
		VkPhysicalDeviceVulkan13Features& features13)
	{
		features13.pNext = &features12;
		features12.pNext = nullptr;

		if (desc.description.features.multiDrawIndirect)
			featuresCore.multiDrawIndirect = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.0 MULTI-DRAW INDIRECT NOT SUPPORTED!");

		if (desc.description.features.samplerAnisotropy)
			featuresCore.samplerAnisotropy = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.0 SAMPLER ANISOTROPY NOT SUPPORTED!");

		if (desc.description.features.drawIndirectFirstInstance)
			featuresCore.drawIndirectFirstInstance = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.0 DRAW INDIRECT FIRST INSTANCE NOT SUPPORTED!");

		if (desc.description.features12.descriptorIndexing)
			features12.descriptorIndexing = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 DESCRIPTOR INDEXING NOT SUPPORTED!");

		if (desc.description.features12.shaderSampledImageArrayNonUniformIndexing)
			features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 SHADER SAMPLED IMAGE ARRAY NON UNIFORM INDEXING NOT SUPPORTED!");

		if (desc.description.features12.runtimeDescriptorArray)
			features12.runtimeDescriptorArray = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 RUNTIME DESCRIPTOR ARRAY NOT SUPPORTED!");

		if (desc.description.features12.descriptorBindingSampledImageUpdateAfterBind)
			features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 DESCRIPTOR BINDING SAMPLED IMAGE UPDATE AFTER BIND NOT SUPPORTED!");

		if (desc.description.features12.descriptorBindingVariableDescriptorCount)
			features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 DESCRIPTOR BINDING VARIABLE DESCRIPTOR COUNT NOT SUPPORTED!");

		if (desc.description.features12.descriptorBindingPartiallyBound)
			features12.descriptorBindingPartiallyBound = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.2 DESCRIPTOR BINDING PARTIALLY BOUND NOT SUPPORTED!");

		if (desc.description.features13.dynamicRendering)
			features13.dynamicRendering = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.3 DYNAMIC RENDERING NOT SUPPORTED!");

		if (desc.description.features13.synchronization2)
			features13.synchronization2 = VK_TRUE;
		else throw std::runtime_error("VULKAN 1.3 SYNC2 NOT SUPPORTED!");
	}


	static DeviceLimits toDeviceLimits(const VkPhysicalDeviceLimits& vkLimits, const VkPhysicalDeviceVulkan12Properties& props12)
	{
		MemoryLimits memLimits{ vkLimits.maxMemoryAllocationCount };

		CommandLimits cmdLimits
		{
			.maxDrawIndirectCount = vkLimits.maxDrawIndirectCount,
			.maxPushConstantsSize = vkLimits.maxPushConstantsSize,
			.maxBoundDescriptorSets = vkLimits.maxBoundDescriptorSets
		};

		BufferLimits bufferLimits
		{
			.minUniformBufferOffsetAlignment = vkLimits.minUniformBufferOffsetAlignment,
			.minStorageBufferOffsetAlignment = vkLimits.minStorageBufferOffsetAlignment,
			.maxUniformBufferRange = vkLimits.maxUniformBufferRange,
			.maxStorageBufferRange = vkLimits.maxStorageBufferRange
		};

		ImageLimits imageLimits
		{
			.maxImageDimension2D = vkLimits.maxImageDimension2D,
			.maxImageDimensionCube = vkLimits.maxImageDimensionCube,
			.maxSamplerAnisotropy = vkLimits.maxSamplerAnisotropy
		};

		ComputeLimits computeLimits
		{
			.maxComputeWorkGroupInvocations = vkLimits.maxComputeWorkGroupInvocations,
			.maxComputeSharedMemorySize = vkLimits.maxComputeSharedMemorySize
		};

		memcpy(computeLimits.maxComputeWorkGroupCount.data(), vkLimits.maxComputeWorkGroupCount, sizeof vkLimits.maxComputeWorkGroupCount);
		memcpy(computeLimits.maxComputeWorkGroupSize.data(), vkLimits.maxComputeWorkGroupSize, sizeof vkLimits.maxComputeWorkGroupSize);


		FramebufferLimits framebuffer
		{
			.colorSampleCounts = utils::toSampleCount(vkLimits.framebufferColorSampleCounts),
			.depthSampleCounts = utils::toSampleCount(vkLimits.framebufferDepthSampleCounts),
			.stencilSampleCounts = utils::toSampleCount(vkLimits.framebufferStencilSampleCounts)
		};


		DescriptorLimits descLimits
		{
			.maxPerStageUpdateAfterBindUniformBuffers =
				props12.maxPerStageDescriptorUpdateAfterBindUniformBuffers,
			.maxDescriptorSetUpdateAfterBindUniformBuffers =
				props12.maxDescriptorSetUpdateAfterBindUniformBuffers,

			.maxPerStageUpdateAfterBindStorageBuffers =
				props12.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
			.maxDescriptorSetUpdateAfterBindStorageBuffers =
				props12.maxDescriptorSetUpdateAfterBindStorageBuffers,

			.maxPerStageUpdateAfterBindStorageImages =
				props12.maxPerStageDescriptorUpdateAfterBindStorageImages,
			.maxDescriptorSetUpdateAfterBindStorageImages =
				props12.maxDescriptorSetUpdateAfterBindStorageImages,

			.maxPerStageUpdateAfterBindSampledImages =
				props12.maxPerStageDescriptorUpdateAfterBindSampledImages,
			.maxDescriptorSetUpdateAfterBindSampledImages =
				props12.maxDescriptorSetUpdateAfterBindSampledImages,
		};

		return DeviceLimits
		{
			.memory = memLimits,
			.command = cmdLimits,
			.buffer = bufferLimits,
			.image = imageLimits,
			.compute = computeLimits,
			.descriptor = descLimits,
			.framebuffer = framebuffer
		};
	}

	struct Device::Impl 
	{
		DeviceLimits limits{};

		std::string deviceName;

		VkPhysicalDevice physical = nullptr;
		VkDevice logical = nullptr;
		
		std::unique_ptr<QueueFamilyList> queueFamilyList;
	};

	Device::~Device()
	{
		if (m_impl && m_impl->logical)
		{
			vkDestroyDevice(m_impl->logical, nullptr);
		}
	}

	Device::Device(const DeviceDesc& desc) : m_impl(new Impl)
	{
		
		// ===================================================================
		//  Device Queue Configuration
		std::vector<VkDeviceQueueCreateInfo> queueInfos(desc.queueFamilies.size());
		std::vector<std::vector<float_t>> priorityStorage(desc.queueFamilies.size());
		for (size_t i = 0; i < queueInfos.size(); ++i)
		{
			priorityStorage[i].assign(desc.queueFamilies[i].count, 1.0f);
			queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfos[i].queueFamilyIndex = desc.queueFamilies[i].family;
			queueInfos[i].queueCount = desc.queueFamilies[i].count;
			queueInfos[i].pQueuePriorities = priorityStorage[i].data();
		}
		// ===================================================================


		// ===================================================================
		// Physical Device Features Configuration
		// ===================================================================
		VkPhysicalDeviceFeatures featuresCore{};
		VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		chooseDefaultFeatures(desc, featuresCore, features12, features13);
		// ===================================================================

		VkDeviceCreateInfo deviceInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features13,  // Vk Secondary Features
			.queueCreateInfoCount = (uint32_t) queueInfos.size(),
			.pQueueCreateInfos = queueInfos.data(),
			.enabledExtensionCount = desc.enabledExtensionCount,
			.ppEnabledExtensionNames = desc.ppEnabledExtensionNames,
			.pEnabledFeatures = &featuresCore, // Vk Core Features
		};

		VK_CHECK(vkCreateDevice(desc.description.physical, &deviceInfo, nullptr, &m_impl->logical));

		m_impl->physical = desc.description.physical;
		m_impl->deviceName = desc.description.name;
		m_impl->limits = toDeviceLimits(desc.description.limits, desc.description.properties12);
		m_impl->queueFamilyList = std::make_unique<QueueFamilyList>(*this, desc.queueFamilies);
	}

	void Device::wait()
	{
		VK_CHECK(vkDeviceWaitIdle(m_impl->logical));
	}

	const std::string& Device::name() const
	{
		return m_impl->deviceName;
	}

	DeviceLimits Device::limits() const
	{
		return m_impl->limits;
	}

	QueueFamilyList& Device::queueFamilyList() const
	{
		return *(m_impl->queueFamilyList);
	}

	AdapterHandle Device::adapterHandle() const
	{
		return { reinterpret_cast<uint64_t>(m_impl->physical) };
	}

	DeviceHandle Device::handle() const
	{
		return { reinterpret_cast<uint64_t>(m_impl->logical) };
	}

}
