#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>
#include <window/WindowBase.hpp>

namespace tungol::runtime
{
	struct TUNGOL_EXPORT RenderKernel
	{
	public:
		RenderKernel(WindowBase& window);
		~RenderKernel();
	private:
		static constexpr uint32_t FramesInFlight = 4;

		friend class Renderer;
		friend class RenderScene;
		friend class AnimationResources;
		friend class ShadowResources;

		vhl::DescriptorPool& descriptorPool() const;
		vhl::DescriptorSetLayoutList& layouts() const;
		vhl::SegregatedAllocator& allocator() const;
		vhl::HazardTracker& tracker() const;
		vhl::CommandQueue& mainQueue() const;
		vhl::GraphicsContext& context() const;

		WindowBase& window() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
