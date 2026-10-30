#pragma once
#include "CommandBuffer.hpp"

#include <vhl/core/images/Image.hpp>
#include <vhl/core/buffers/Buffer.hpp>

namespace tungol::vhl 
{

	struct ImageBarrierDesc
	{
		PipelineStageFlags srcStageMask;
		AccessFlags srcAccessMask;

		PipelineStageFlags dstStageMask;
		AccessFlags dstAccessMask;

		Image* image = nullptr;

		ImageLayout oldLayout;
		ImageLayout newLayout;

		QueueFamily srcQueueFamily = QueueFamilyIgnored;

		QueueFamily dstQueueFamily = QueueFamilyIgnored;
		
		ImageAspectFlags aspect = ImageAspectFlags::Color;

		uint32_t baseMipLevel = 0;
		uint32_t levelCount = RemainingMipLevels;
		uint32_t baseLayer = 0;
		uint32_t layerCount = RemainingArrayLayers;
	};

	struct BufferBarrierDesc
	{
		PipelineStageFlags srcStageMask;
		AccessFlags srcAccessMask;

		PipelineStageFlags dstStageMask;
		AccessFlags dstAccessMask;

		Buffer* buffer = nullptr;

		uint64_t offset = 0;
		uint64_t size = BufferWholeSize;

		QueueFamily srcQueueFamily = QueueFamilyIgnored;
		QueueFamily dstQueueFamily = QueueFamilyIgnored;
	};

	struct MemoryBarrierDesc
	{
		PipelineStageFlags srcStageMask, dstStageMask;
		AccessFlags srcAccessMask, dstAccessMask;

		QueueFamily srcQueueFamily = QueueFamilyIgnored;
		QueueFamily dstQueueFamily = QueueFamilyIgnored;
	};



	class TUNGOL_EXPORT BarrierCommand
	{
	public:
		BarrierCommand() = delete;

		static void image(const CommandBuffer& commandBuffer, const ImageBarrierDesc&);

		static void buffer(const CommandBuffer& commandBuffer, const BufferBarrierDesc&);

		static void memory(const CommandBuffer& commandBuffer, const MemoryBarrierDesc&);
	};
}
