#include "GeometryPool.hpp"

#include <vhl/core/Allocator.hpp>

#include <vhl/core/buffers/VertexBuffer.hpp>
#include <vhl/core/buffers/IndexBuffer.hpp>



namespace tungol::runtime 
{
	struct GeometryPool::Impl 
	{
	public:
		Impl(vhl::Device& device, vhl::SegregatedAllocator& allocator, uint64_t size);
		~Impl();
	private:
		std::unique_ptr<vhl::VertexBuffer> m_staticVB;
		std::unique_ptr<vhl::VertexBuffer> m_skinnedVB;

		std::unique_ptr<vhl::IndexBuffer> m_indexBuf;
	};

	GeometryPool::Impl::Impl(vhl::Device& device, vhl::SegregatedAllocator& allocator, uint64_t size) :
		m_staticVB(std::make_unique<vhl::VertexBuffer>(device, allocator, size)),
		m_skinnedVB(std::make_unique<vhl::VertexBuffer>(device, allocator, size)),
		m_indexBuf(std::make_unique<vhl::IndexBuffer>(device, allocator, size))
	{
	}

	GeometryPool::Impl::~Impl()
	{
		m_staticVB.reset();
		m_skinnedVB.reset();
		m_indexBuf.reset();
	}

	GeometryPool::GeometryPool(vhl::Device& device, vhl::SegregatedAllocator& allocator, uint64_t size) 
		: m_impl(new Impl(device, allocator, size))
	{
	
	}
	
}