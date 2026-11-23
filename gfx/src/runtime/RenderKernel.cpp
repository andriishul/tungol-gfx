#include "RenderKernel.hpp"

#include "internal/KernelInternal.hpp"

#include <event/EventBus.hpp>
#include <event/GraphicsEventArgs.hpp>
#include "helper/CatchGraphicsError.hpp"

#include "ShadowMap.hpp"

#include "RenderConfig.hpp"

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>
#include <vhl/graph/CommandQueue.hpp>

#include "ResidencyManager.hpp"
#include <core/AssetCache.hpp>

#include <sstream>
#include <stdexcept>

namespace tungol::runtime
{
	using namespace vhl;

	struct RenderKernel::Impl
	{
		Impl(WindowBase& w, core::AssetCache& a) : window(w), assets(a) {}

		WindowBase& window;
		core::AssetCache& assets;

		std::unique_ptr<GraphicsContext> context;

		std::unique_ptr<HazardTracker> tracker;
		std::unique_ptr<CommandQueue> directQueue;

		std::unique_ptr<SegregatedAllocator> allocator;

		std::unique_ptr<DescriptorPool> descriptorPool;
		std::unique_ptr<DescriptorSetLayoutList> layouts;

		std::unique_ptr<ResidencyManager> residencyManager;

		void initDescriptors(const Device& device, uint32_t totalSampledImages);
	};

	void RenderKernel::Impl::initDescriptors(const Device& device, uint32_t totalSampledImages)
	{
		layouts = std::make_unique<DescriptorSetLayoutList>(device);

		// Tag 0: view data (camera, shadow, light) (uniform buffer, vertex)
		DescriptorBinding bindingViewUBO{ 0, 1, DescriptorType::UniformBuffer, ShaderStageFlags::Vertex };

		// Tag 1: per-frame object data — instances only (vertex)
		DescriptorBinding bindingInstances{ 0, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Vertex };

		// Tag 2: persistent material data — materials (fragment) + textures (fragment)
		DescriptorBinding bindingMaterials{ 0, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Fragment };
		DescriptorBinding bindingSamplers{ 1, totalSampledImages - 1, DescriptorType::SampledImage, ShaderStageFlags::Fragment, true, true, false };

		// Tag 3: sample compute only
		DescriptorBinding bindingBoneMeta{ 0, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingPlayback{ 1, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingAnimations{ 2, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingChannels{ 3, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingKeyframes{ 4, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingBindPoses{ 5, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingBoneLocals{ 6, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };

		// Tag 4: hierarchy compute only
		DescriptorBinding bindingHierMeta{ 0, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingHierPlayback{ 1, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingHierLocals{ 6, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingHierPalette{ 7, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute | ShaderStageFlags::Vertex };
		DescriptorBinding bindingHierInverseBind{ 8, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };

		// Tag 5: shadow map (fragment read)
		DescriptorBinding bindingShadowMap{ 0, 1, DescriptorType::SampledImage, ShaderStageFlags::Fragment };

		layouts->append(0, { bindingViewUBO });
		layouts->append(1, { bindingInstances });
		layouts->append(2, { bindingMaterials, bindingSamplers });
		layouts->append(3, { bindingBoneMeta, bindingPlayback, bindingAnimations, bindingChannels, bindingKeyframes, bindingBindPoses, bindingBoneLocals });

		layouts->append(4, { bindingHierMeta, bindingHierPlayback, bindingHierLocals, bindingHierPalette, bindingHierInverseBind });
		layouts->append(5, { bindingShadowMap });

		const std::vector<DescriptorPoolSize> poolSizes
		{
			{ DescriptorType::UniformBuffer, FramesInFlight },
			{ DescriptorType::StorageBuffer, 1024  },
			{ DescriptorType::SampledImage, totalSampledImages },
		};

		descriptorPool = std::make_unique<DescriptorPool>(device, 128, poolSizes, true);
	}

	RenderKernel::RenderKernel(WindowBase& window, core::AssetCache& assets) : m_impl(new Impl(window, assets))
	{
		m_impl->context = std::make_unique<GraphicsContext>(window);

		auto& device = m_impl->context->activeDevice();
		auto& swapchain = m_impl->context->swapchain();

		m_impl->tracker = std::make_unique<HazardTracker>();
		m_impl->directQueue = std::make_unique<CommandQueue>(device, FramesInFlight, swapchain.colorImageCount());

		m_impl->allocator = SegregatedAllocator::create(device);

		auto descriptorLimits = device.limits().descriptor;

		const uint32_t totalSampledImages = main::k_MaxTextures + 1; // shadow map

		if (totalSampledImages > descriptorLimits.maxDescriptorSetUpdateAfterBindSampledImages)
		{
			std::ostringstream ss;
			ss << "[RenderKernel] Total sampled images exceed device limit.\n"
				<< "  Required  : " << totalSampledImages
				<< "  Device max: " << descriptorLimits.maxDescriptorSetUpdateAfterBindSampledImages;
			throw std::runtime_error(ss.str());
		}

		m_impl->initDescriptors(device, totalSampledImages);

		m_impl->residencyManager = std::make_unique<ResidencyManager>(*this, totalSampledImages);
	}

	RenderKernel::~RenderKernel()
	{
		try
		{
			m_impl->context->activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR
		{
			m_impl->residencyManager.reset();

			m_impl->descriptorPool.reset();
			m_impl->layouts.reset();
			m_impl->allocator.reset();
			m_impl->tracker.reset();
			m_impl->directQueue.reset();
			m_impl->context.reset();
		}
	}
	


	core::AssetCache& RenderKernel::assets() const
	{
		return m_impl->assets;
	}

	
	WindowBase& RenderKernel::window() const
	{
		return m_impl->window;
	}


	DescriptorPool& KernelInternal::descriptorPool(RenderKernel& rk)
	{
		return *rk.m_impl->descriptorPool;
	}
	DescriptorSetLayoutList& KernelInternal::layouts(RenderKernel& rk)
	{
		return *rk.m_impl->layouts;
	}
	SegregatedAllocator& KernelInternal::allocator(RenderKernel& rk)
	{
		return *rk.m_impl->allocator;
	}
	HazardTracker& KernelInternal::tracker(RenderKernel& rk)
	{
		return *rk.m_impl->tracker;
	}
	CommandQueue& KernelInternal::directQueue(RenderKernel& rk)
	{
		return *rk.m_impl->directQueue;
	}
	GraphicsContext& KernelInternal::context(RenderKernel& rk)
	{
		return *rk.m_impl->context;
	}

	ResidencyManager& KernelInternal::residency(RenderKernel& rk)
	{
		return *rk.m_impl->residencyManager;
	}
}
