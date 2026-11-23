#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

namespace tungol::runtime 
{
	struct TUNGOL_EXPORT KernelInternal 
	{
		static vhl::DescriptorPool& descriptorPool(RenderKernel& kernel);
		static vhl::DescriptorSetLayoutList& layouts(RenderKernel& kernel);
		static vhl::SegregatedAllocator& allocator(RenderKernel& kernel);
		static vhl::HazardTracker& tracker(RenderKernel& kernel);
		static vhl::CommandQueue& directQueue(RenderKernel& kernel);
		static vhl::GraphicsContext& context(RenderKernel& kernel);
		
		static ResidencyManager& residency(RenderKernel& kernel);
	};
}