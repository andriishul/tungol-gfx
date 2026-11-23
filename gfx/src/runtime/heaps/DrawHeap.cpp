#include "DrawHeap.hpp"

#include <runtime/RenderKernel.hpp>
#include <runtime/internal/KernelInternal.hpp>

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>
#include <vhl/core/Allocator.hpp>

#include <vhl/core/Descriptors.hpp>

#include <vhl/core/buffers/IndirectBuffer.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

namespace tungol::runtime
{
	struct DrawHeap::Impl
	{
		Impl(RenderKernel& kernel, const DrawHeapSizes& sizes);
		~Impl();

		vhl::DescriptorPool& descPool;
		DrawHeapSizes heapSizes;

		std::unique_ptr<vhl::IndirectBuffer> indirectBufs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StorageBuffer> instancesSBs[RenderKernel::FramesInFlight];

		std::unique_ptr<vhl::StagingBuffer> indirectTBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StagingBuffer> instancesTBs[RenderKernel::FramesInFlight];

		vhl::DescriptorSetHandle objectSets[RenderKernel::FramesInFlight];
	};

	DrawHeap::Impl::Impl(RenderKernel& kern, const DrawHeapSizes& s)
		: descPool(KernelInternal::descriptorPool(kern)), heapSizes(s)
	{
		auto& device = KernelInternal::context(kern).activeDevice();
		auto& allocator = KernelInternal::allocator(kern);
		auto& tracker = KernelInternal::tracker(kern);

		auto& layouts = KernelInternal::layouts(kern);

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
		{
			indirectBufs[i] = std::make_unique<vhl::IndirectBuffer>(device, allocator, s.indirectSize);
			instancesSBs[i] = std::make_unique<vhl::StorageBuffer>(device, allocator, s.instancesSize);
			
			indirectTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, s.indirectSize);
			instancesTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, s.instancesSize);

			tracker.emplace(*indirectBufs[i]);
			tracker.emplace(*instancesSBs[i]);

			tracker.emplace(*indirectTBs[i]);
			tracker.emplace(*instancesTBs[i]);

			objectSets[i] = descPool.emplace(layouts[1]);

			vhl::DescriptorWriter writer(objectSets[i]);
			writer.storageBuffer(*instancesSBs[i], 0, 0, s.instancesSize);
			writer.apply(device);
		}
	}


	DrawHeap::Impl::~Impl() 
	{
		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i) 
		{
			descPool.remove(objectSets[i]);

			indirectBufs[i].reset();
			instancesSBs[i].reset();

			indirectTBs[i].reset();
			instancesTBs[i].reset();
		}
	}


	DrawHeap::DrawHeap(RenderKernel& kernel, const DrawHeapSizes& sizes)
		: m_impl(new Impl(kernel, sizes))
	{
	}

	DrawHeap::~DrawHeap() = default;

	vhl::Buffer& DrawHeap::indirectBufs(uint32_t slot) const 
	{
		return *m_impl->indirectBufs[slot]; 
	}
	vhl::Buffer& DrawHeap::instancesSBs(uint32_t slot) const 
	{
		return *m_impl->instancesSBs[slot]; 
	}
	
	vhl::Buffer& DrawHeap::indirectTBs(uint32_t slot) const 
	{
		return *m_impl->indirectTBs[slot]; 
	}
	vhl::Buffer& DrawHeap::instancesTBs(uint32_t slot) const 
	{
		return *m_impl->instancesTBs[slot]; 
	}
	
	const vhl::DescriptorSetHandle& DrawHeap::objectSets(uint32_t slot) const
	{
		return m_impl->objectSets[slot];
	}
	DrawHeapSizes DrawHeap::sizes() const
	{
		return m_impl->heapSizes;
	}
}