#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <runtime/ResidencyManager.hpp>
#include <window/WindowBase.hpp>


namespace tungol::runtime
{
	struct TUNGOL_EXPORT RenderKernel
	{
	public:
		static constexpr uint32_t FramesInFlight = 4U;

		RenderKernel(WindowBase& window, core::AssetCache& assets);
		~RenderKernel();

		core::AssetCache& assets() const;
		
		WindowBase& window() const;
	private:
		friend struct KernelInternal;
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
