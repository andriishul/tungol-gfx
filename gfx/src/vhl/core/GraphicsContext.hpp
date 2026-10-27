#pragma once
#include <window/WindowBase.hpp>

#include "Swapchain.hpp"
#include "Device.hpp"

/*
	VHL - Vulkan Hardware Layer
*/
namespace tungol::vhl 
{
	class TUNGOL_EXPORT GraphicsContext
	{
	public:
		GraphicsContext() = delete;
		GraphicsContext(const WindowBase&);
		~GraphicsContext();

		Device& activeDevice() const;
		Swapchain& swapchain() const; 

		void rebuildSwapchain();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
