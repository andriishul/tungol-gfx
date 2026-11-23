#include "GeometryHeap.hpp"

#include <runtime/RenderKernel.hpp>
#include <runtime/internal/KernelInternal.hpp>

#include <asset/ModelFormat.hpp>

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>

#include <vhl/core/Allocator.hpp>

#include <vhl/core/buffers/VertexBuffer.hpp>
#include <vhl/core/buffers/IndexBuffer.hpp>

namespace tungol::runtime
{
	struct GeometryHeap::Impl
	{
		Impl(RenderKernel& kernel, const GeometryHeapSizes& sizes);
		~Impl();

		GeometryHeapSizes heapSizes;

		GeometryAllocation allocate(const GeometrySizes& sizes);
		void free(const GeometryAllocation& a);

		std::unique_ptr<Pool> staticPool;
		std::unique_ptr<Pool> skinnedPool;
		std::unique_ptr<Pool> indexedPool;
	};

	GeometryHeap::Impl::Impl(RenderKernel& kern, const GeometryHeapSizes& s) : heapSizes(s)
	{
		auto& device = KernelInternal::context(kern).activeDevice();
		auto& allocator = KernelInternal::allocator(kern);
		auto& tracker = KernelInternal::tracker(kern);

		std::unique_ptr<vhl::Buffer> staticVB(new vhl::VertexBuffer(device, allocator, s.staticVBSize));
		tracker.emplace(*staticVB);
		staticPool = std::make_unique<Pool>(std::move(staticVB), s.staticVBSize, sizeof(asset::StaticVertex));

		std::unique_ptr<vhl::Buffer> skinnedVB(new vhl::VertexBuffer(device, allocator, s.skinnedVBSize));
		tracker.emplace(*skinnedVB);
		skinnedPool = std::make_unique<Pool>(std::move(skinnedVB), s.skinnedVBSize, sizeof(asset::SkinnedVertex));

		std::unique_ptr<vhl::Buffer> indexBuf(new vhl::IndexBuffer(device, allocator, s.indexBufSize));
		tracker.emplace(*indexBuf);
		indexedPool = std::make_unique<Pool>(std::move(indexBuf), s.indexBufSize, sizeof(uint32_t));
	}

	GeometryHeap::Impl::~Impl()
	{
		staticPool.reset();
		skinnedPool.reset();
		indexedPool.reset();
	}

	GeometryAllocation GeometryHeap::Impl::allocate(const GeometrySizes& sizes)
	{
		GeometryAllocation allocation{};
		if (sizes.staticVBSize > 0 && staticPool->canAllocate(sizes.staticVBSize)) 
			allocation.staticVB = *staticPool->allocate(sizes.staticVBSize);
		if (sizes.skinnedVBSize > 0 && skinnedPool->canAllocate(sizes.skinnedVBSize)) 
			allocation.skinnedVB = *skinnedPool->allocate(sizes.skinnedVBSize);
		if (sizes.indexBufSize > 0 && indexedPool->canAllocate(sizes.indexBufSize)) 
			allocation.index = *indexedPool->allocate(sizes.indexBufSize);

		return allocation;
	}

	void GeometryHeap::Impl::free(const GeometryAllocation& a)
	{
		if (a.staticVB.has_value() && a.staticVB->size > 0) staticPool->free(*a.staticVB);
		if (a.skinnedVB.has_value() && a.skinnedVB->size > 0) skinnedPool->free(*a.skinnedVB);
		if (a.index.has_value() && a.index->size > 0) indexedPool->free(*a.index);
	}

	GeometryHeap::GeometryHeap(RenderKernel& kernel, const GeometryHeapSizes& sizes)
		: m_impl(new Impl(kernel, sizes))
	{

	}

	GeometryHeap::~GeometryHeap() = default;

	GeometryAllocation GeometryHeap::allocate(const GeometrySizes& sizes)
	{
		return m_impl->allocate(sizes);
	}

	void GeometryHeap::free(const GeometryAllocation& a)
	{
		m_impl->free(a);
	}

	const Pool& GeometryHeap::staticPool() const
	{
		return *m_impl->staticPool;
	}

	const Pool& GeometryHeap::skinnedPool() const
	{
		return *m_impl->skinnedPool;
	}

	const Pool& GeometryHeap::indexedPool() const
	{
		return *m_impl->indexedPool;
	}

	GeometryHeapSizes GeometryHeap::sizes() const
	{
		return m_impl->heapSizes;
	}
}