#include "BarrierCommand.hpp"

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{

	void BarrierCommand::image(const CommandBuffer& commandBuffer, const ImageBarrierDesc& desc)
	{
		VkImage vkImage = nullptr;

		VkCommandBuffer vkCommandBuffer = nullptr;

		if (uint64_t handle_value = commandBuffer.handle().value; handle_value != 0)
		{
			vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);
		}
		else throw std::runtime_error("[BarrierCommand::image] Null CommandBuffer handle.");


		if (desc.image)
		{
			if (uint64_t handle_value = desc.image->handle().value; handle_value != 0)
			{
				vkImage = reinterpret_cast<VkImage>(handle_value);
			}
			else throw std::runtime_error("[BarrierCommand::image] Null image handle.");
		}
		else throw std::runtime_error("[BarrierCommand::image] Pointer Image is null.");

		VkImageMemoryBarrier2 imageBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			
			.srcStageMask = utils::toVkPipelineStageFlags2(desc.srcStageMask),
			.srcAccessMask = utils::toVkAccessFlags2(desc.srcAccessMask),

			.dstStageMask = utils::toVkPipelineStageFlags2(desc.dstStageMask),
			.dstAccessMask = utils::toVkAccessFlags2(desc.dstAccessMask),

			.oldLayout = utils::toVkImageLayout(desc.oldLayout),
			.newLayout = utils::toVkImageLayout(desc.newLayout),

			.srcQueueFamilyIndex = (desc.srcQueueFamily == QueueFamilyIgnored) ? VK_QUEUE_FAMILY_IGNORED : desc.srcQueueFamily,
			.dstQueueFamilyIndex = (desc.dstQueueFamily == QueueFamilyIgnored) ? VK_QUEUE_FAMILY_IGNORED : desc.dstQueueFamily,

			.image = vkImage,

			.subresourceRange = VkImageSubresourceRange
			{
				.aspectMask = utils::toVkImageAspectFlags(desc.aspect),
				
				.baseMipLevel = desc.baseMipLevel,
				.levelCount = desc.levelCount != RemainingMipLevels ? desc.levelCount : VK_REMAINING_MIP_LEVELS,

				.baseArrayLayer = desc.baseLayer,
				.layerCount = desc.layerCount != RemainingArrayLayers ? desc.layerCount : VK_REMAINING_ARRAY_LAYERS
			}
		};

		VkDependencyInfo dependencyInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier
		};

		vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);
	}

	void BarrierCommand::buffer(const CommandBuffer& commandBuffer, const BufferBarrierDesc& desc)
	{
		VkBuffer vkBuffer = nullptr;
		VkCommandBuffer vkCommandBuffer = nullptr;

		if (uint64_t handle_value = commandBuffer.handle().value; handle_value != 0)
		{
			vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);
		}
		else throw std::runtime_error("[BarrierCommand::buffer] Null command buffer handle.");

		if (desc.buffer)
		{
			if (uint64_t handle_value = desc.buffer->handle().value; handle_value != 0)
				vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
			else throw std::runtime_error("[BarrierCommand::buffer] Null buffer handle.");
		}
		else throw std::runtime_error("[BarrierCommand::buffer] Pointer Buffer is null.");


		VkBufferMemoryBarrier2 bufferBarrier
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			
			.srcStageMask = utils::toVkPipelineStageFlags2(desc.srcStageMask),
			.srcAccessMask = utils::toVkAccessFlags2(desc.srcAccessMask),

			.dstStageMask = utils::toVkPipelineStageFlags2(desc.dstStageMask),
			.dstAccessMask = utils::toVkAccessFlags2(desc.dstAccessMask),

			.srcQueueFamilyIndex = (desc.srcQueueFamily == QueueFamilyIgnored) ? VK_QUEUE_FAMILY_IGNORED : desc.srcQueueFamily,
			.dstQueueFamilyIndex = (desc.dstQueueFamily == QueueFamilyIgnored) ? VK_QUEUE_FAMILY_IGNORED : desc.dstQueueFamily,

			.buffer = vkBuffer,

			.offset = 0,
			.size = VK_WHOLE_SIZE
		};


		VkDependencyInfo dependencyInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &bufferBarrier
		};

		vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);
	}
	void BarrierCommand::memory(const CommandBuffer& commandBuffer, const MemoryBarrierDesc& desc)
	{
		VkCommandBuffer vkCommandBuffer = nullptr;

		if (uint64_t handle_value = commandBuffer.handle().value; handle_value != 0)
		{
			vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);
		}
		else throw std::runtime_error("[BarrierCommand::memory] Null command buffer handle.");

		VkMemoryBarrier2 memoryBarrier
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			
			.srcStageMask = utils::toVkPipelineStageFlags2(desc.srcStageMask),
			.srcAccessMask = utils::toVkAccessFlags2(desc.srcAccessMask),

			.dstStageMask = utils::toVkPipelineStageFlags2(desc.dstStageMask),
			.dstAccessMask = utils::toVkAccessFlags2(desc.dstAccessMask)
		};

		VkDependencyInfo dependencyInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &memoryBarrier
		};

		vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);
	}
}

