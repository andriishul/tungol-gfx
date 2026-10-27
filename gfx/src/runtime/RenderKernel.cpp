#include "RenderKernel.hpp"

#include <event/EventBus.hpp>
#include <event/GraphicsEventArgs.hpp>
#include "helper/CatchGraphicsError.hpp"

#include "RenderConfig.hpp"

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>
#include <vhl/graph/CommandQueue.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <sstream>
#include <stdexcept>

namespace tungol::runtime
{
	using namespace vhl;

	struct RenderKernel::Impl
	{
		WindowBase& window;

		std::unique_ptr<GraphicsContext> context;

		std::unique_ptr<HazardTracker> tracker;
		std::unique_ptr<CommandQueue> mainQueue;

		std::unique_ptr<SegregatedAllocator> allocator;

		std::unique_ptr<DescriptorPool> descriptorPool;
		std::unique_ptr<DescriptorSetLayoutList> layouts;

		void initDescriptors(const Device& device, uint32_t sampledImagesPerScene, uint32_t totalSampledImages);
	};

	void RenderKernel::Impl::initDescriptors(const Device& device, uint32_t sampledImagesPerScene, uint32_t totalSampledImages)
	{
		layouts = std::make_unique<DescriptorSetLayoutList>(device);

		// Tag 0: camera (uniform buffer, vertex)
		DescriptorBinding bindingCameraUBO{ 0, 1, DescriptorType::UniformBuffer, ShaderStageFlags::Vertex };
		
		// Tag 1: instances (vertex) + materials (fragment)
		DescriptorBinding bindingInstances{ 0, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Vertex };
		DescriptorBinding bindingMaterials{ 1, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Fragment };
		
		// Tag 2: bindless textures — partiallyBound, updateAfterBind
		DescriptorBinding bindingSamplers{ 0, sampledImagesPerScene, DescriptorType::SampledImage, ShaderStageFlags::Fragment, true, true, false };

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
		DescriptorBinding bindingHierLocals{ 6, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		DescriptorBinding bindingHierPalette{ 7, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute | ShaderStageFlags::Vertex };
		DescriptorBinding bindingHierInverseBind{ 8, 1, DescriptorType::StorageBuffer, ShaderStageFlags::Compute };
		
		// Tag 5: shadow map (fragment read)
		DescriptorBinding bindingShadowMap{ 0, 1, DescriptorType::SampledImage, ShaderStageFlags::Fragment };

		layouts->append(0, { bindingCameraUBO });
		layouts->append(1, { bindingInstances, bindingMaterials });
		layouts->append(2, { bindingSamplers });
		layouts->append(3, { bindingBoneMeta, bindingPlayback, bindingAnimations, bindingChannels, bindingKeyframes, bindingBindPoses, bindingBoneLocals });

		layouts->append(4, { bindingHierMeta, bindingHierLocals, bindingHierPalette, bindingHierInverseBind });
		layouts->append(5, { bindingShadowMap });

		const std::vector<DescriptorPoolSize> poolSizes
		{
			{ DescriptorType::UniformBuffer, FramesInFlight * main::k_MaxScenes },
			{ DescriptorType::StorageBuffer, (FramesInFlight * 2 + 8) * main::k_MaxScenes },
			{ DescriptorType::SampledImage, totalSampledImages },
		};

		descriptorPool = std::make_unique<DescriptorPool>(device, 128, poolSizes, true);
	}

	RenderKernel::RenderKernel(WindowBase& window) : m_impl(new Impl{ window })
	{
		m_impl->context = std::make_unique<GraphicsContext>(window);

		auto& device = m_impl->context->activeDevice();
		auto& swapchain = m_impl->context->swapchain();

		m_impl->tracker = std::make_unique<HazardTracker>();
		m_impl->mainQueue = std::make_unique<CommandQueue>(device, FramesInFlight, swapchain.colorImageCount());

		m_impl->allocator = SegregatedAllocator::create(device);

		auto descriptorLimits = device.limits().descriptor;

		const uint32_t sampledImagesPerScene = main::k_MaxBindlessTexturesPerScene + 1;  // + shadow
		const uint32_t totalSampledImages = sampledImagesPerScene * main::k_MaxScenes;

		if (sampledImagesPerScene > descriptorLimits.maxPerStageUpdateAfterBindSampledImages)
		{
			std::ostringstream ss;
			ss << "[RenderKernel] Per-scene sampled images exceed per-stage limit.\n"
				<< "  Required  : " << sampledImagesPerScene << "\n"
				<< "  Device max: " << descriptorLimits.maxPerStageUpdateAfterBindSampledImages << "\n"
				<< "  Fix: lower k_MaxBindlessTexturesPerScene.";
			throw std::runtime_error(ss.str());
		}
		if (totalSampledImages > descriptorLimits.maxDescriptorSetUpdateAfterBindSampledImages)
		{
			std::ostringstream ss;
			ss << "[RenderKernel] Total sampled images exceed device limit.\n"
				<< "  Required  : " << totalSampledImages
				<< " (" << sampledImagesPerScene << " × " << main::k_MaxScenes << " scenes)\n"
				<< "  Device max: " << descriptorLimits.maxDescriptorSetUpdateAfterBindSampledImages << "\n"
				<< "  Fix: lower k_MaxScenes or k_MaxBindlessTexturesPerScene.";
			throw std::runtime_error(ss.str());
		}

		m_impl->initDescriptors(device, sampledImagesPerScene, totalSampledImages);
	}

	RenderKernel::~RenderKernel()
	{
		try
		{
			m_impl->context->activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR

		m_impl->descriptorPool.reset();
		m_impl->layouts.reset();
		m_impl->allocator.reset();
		m_impl->tracker.reset();
		m_impl->mainQueue.reset();
		m_impl->context.reset();
	}
	DescriptorPool& RenderKernel::descriptorPool() const
	{
		return *m_impl->descriptorPool;
	}
	DescriptorSetLayoutList& RenderKernel::layouts() const
	{
		return *m_impl->layouts;
	}
	SegregatedAllocator& RenderKernel::allocator() const
	{
		return *m_impl->allocator;
	}
	HazardTracker& RenderKernel::tracker() const
	{
		return *m_impl->tracker;
	}
	CommandQueue& RenderKernel::mainQueue() const
	{
		return *m_impl->mainQueue;
	}
	GraphicsContext& RenderKernel::context() const
	{
		return *m_impl->context;
	}

	WindowBase& RenderKernel::window() const
	{
		return m_impl->window;
	}
}
