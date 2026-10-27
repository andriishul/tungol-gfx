#pragma once
#include <cstdint>

#define TUNGOL_DEFINE_HANDLE(name) struct name { uint64_t value = 0; }

namespace tungol::vhl
{
	TUNGOL_DEFINE_HANDLE(PipelineLayoutHandle);
	TUNGOL_DEFINE_HANDLE(PipelineHandle);

	TUNGOL_DEFINE_HANDLE(DescriptorSetHandle);

	TUNGOL_DEFINE_HANDLE(DescriptorSetLayoutHandle);
	TUNGOL_DEFINE_HANDLE(CommandBufferHandle);

	TUNGOL_DEFINE_HANDLE(SemaphoreHandle);
	TUNGOL_DEFINE_HANDLE(FenceHandle);

	TUNGOL_DEFINE_HANDLE(AdapterHandle);
	TUNGOL_DEFINE_HANDLE(DeviceHandle);
	TUNGOL_DEFINE_HANDLE(SwapchainHandle);

	TUNGOL_DEFINE_HANDLE(ImageHandle);
	TUNGOL_DEFINE_HANDLE(ImageViewHandle);
	TUNGOL_DEFINE_HANDLE(SamplerHandle);

	TUNGOL_DEFINE_HANDLE(BufferHandle);

	TUNGOL_DEFINE_HANDLE(DeviceMemoryHandle);
}
