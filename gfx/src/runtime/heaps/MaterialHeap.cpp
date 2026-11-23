#include "MaterialHeap.hpp"

#include <runtime/RenderKernel.hpp>
#include <runtime/internal/KernelInternal.hpp>
#include <runtime/data/MaterialData.hpp>

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/core/Allocator.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>

#include <stdexcept>

namespace tungol::runtime
{
	struct MaterialHeap::Impl 
	{
		Impl(RenderKernel& kernel, const MaterialHeapSizes& sizes);
		~Impl();

		MaterialHeapSizes heapSize;

		std::unique_ptr<Pool> materialsPool;
	};

	MaterialHeap::Impl::Impl(RenderKernel& kernel, const MaterialHeapSizes& sizes) : heapSize(sizes)
	{
		auto& device = KernelInternal::context(kernel).activeDevice();
		auto& allocator = KernelInternal::allocator(kernel);
		auto& tracker = KernelInternal::tracker(kernel);

		std::unique_ptr<vhl::Buffer> materialsSB(new vhl::StorageBuffer(device, allocator, sizes.materialsSize));
		tracker.emplace(*materialsSB);
		materialsPool = std::make_unique<Pool>(std::move(materialsSB), sizes.materialsSize, sizeof(ShaderMaterial));
	}

	MaterialHeap::Impl::~Impl() 
	{
		materialsPool.reset();
	}

	MaterialHeap::~MaterialHeap() = default;

	MaterialHeap::MaterialHeap(RenderKernel& kernel, const MaterialHeapSizes& sizes) : m_impl(new Impl(kernel, sizes))
	{
	}

	MaterialAllocation MaterialHeap::allocate(uint64_t size)
	{
		MaterialAllocation allocation{};
		if (size > 0 && m_impl->materialsPool->canAllocate(size))
			allocation.materials = *m_impl->materialsPool->allocate(size);
		return allocation;
	}

	void MaterialHeap::free(const MaterialAllocation& a)
	{
		if (a.materials.has_value())
			m_impl->materialsPool->free(*a.materials);
	}


	const Pool& MaterialHeap::pool() const
	{
		return *m_impl->materialsPool;
	}

	MaterialHeapSizes MaterialHeap::sizes() const
	{
		return m_impl->heapSize;
	}
	
}