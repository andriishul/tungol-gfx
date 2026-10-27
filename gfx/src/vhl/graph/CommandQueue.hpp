#pragma once
#include <core/Fwd.hpp>
#include <core/TungolExport.hpp>

#include <vhl/core/GraphicsCommon.hpp>

namespace tungol::vhl
{
	class TUNGOL_EXPORT CommandQueue
	{
	public:
		// Creates a queue that accepts Graphics, Compute, Transfer, and Present work.
		// Internally selects the first device queue family that supports all four flags.
		// This is the DX12 `DIRECT` / Metal unified queue: rendering, compute, and uploads all submit through a single queue. 
		CommandQueue(Device& device, uint32_t framesInFlight, uint32_t swapchainImageCount);
		~CommandQueue();

		void resetCmd();

		CommandBuffer& cmd() const;
		Queue& native() const;
		QueueFamily family() const;

		void begin();

		AcquireResult nextImageIndex(const Swapchain& swapchain);

		void submit();
		bool end(const Swapchain& swapchain, uint32_t imageIndex);

		uint32_t frameSlot() const;

		void wait();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
