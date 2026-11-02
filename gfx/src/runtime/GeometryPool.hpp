#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

namespace tungol::runtime 
{
	class TUNGOL_EXPORT GeometryPool
	{
	public:
		GeometryPool(vhl::Device& device, vhl::SegregatedAllocator& allocator, uint64_t size);
		~GeometryPool();

		vhl::Buffer& staticVertex();
		vhl::Buffer& skinnedVertex();
		vhl::Buffer& indexBuf();

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}