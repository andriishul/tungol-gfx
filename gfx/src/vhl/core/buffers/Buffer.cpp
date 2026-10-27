#include "Buffer.hpp"

#include <stdexcept>

namespace tungol::vhl {

	struct Buffer::Impl 
	{
		const Device& device;
		SegregatedAllocator& allocator;

		BufferType bufferType = BufferType::Vertex;

		buffer::BufferResource resource{};

		uint64_t size = 0;

		BufferLimits limits{};
	};

	Buffer::Buffer(const Device& device, SegregatedAllocator& allocator, const BufferDesc& desc) : m_impl(new Impl{ device, allocator })
	{
		uint32_t maxUniformBufferRange = device.limits().buffer.maxUniformBufferRange;
		uint32_t maxStorageBufferRange = device.limits().buffer.maxStorageBufferRange;

		if (desc.bufferType == BufferType::Uniform && maxUniformBufferRange < desc.size)
		{
			throw std::runtime_error("[Buffer::Buffer] Uniform buffer size exceeds hardware limit of " + std::to_string(maxUniformBufferRange));
		}
		else if (desc.bufferType == BufferType::Storage && maxStorageBufferRange < desc.size)
		{
			throw std::runtime_error("[Buffer::Buffer] Storage buffer size exceeds hardware limit of " + std::to_string(maxStorageBufferRange));
		}

		m_impl->bufferType = desc.bufferType;
		m_impl->size = desc.size;
		m_impl->limits = device.limits().buffer;

		m_impl->resource = buffer::create(device, allocator, desc);
	}


	Buffer::~Buffer()
	{
		if (dispose) dispose(this);
		buffer::destroy(m_impl->device, m_impl->allocator, m_impl->resource);
	}

	void* Buffer::pMappedData() const 
	{
		return m_impl->resource.pMappedData;
	}

	BufferType Buffer::type() const
	{
		return m_impl->bufferType;
	}

	uint64_t Buffer::size() const
	{
		return m_impl->size;
	}

	BufferHandle Buffer::handle() const
	{
		return m_impl->resource.handle;
	}

	BufferLimits Buffer::limits() const
	{
		return m_impl->limits;
	}
}
