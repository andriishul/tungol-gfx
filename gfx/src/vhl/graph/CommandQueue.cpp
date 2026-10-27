#include "CommandQueue.hpp"

#include <vhl/commands/CommandBuffer.hpp>
#include <vhl/core/Swapchain.hpp>

#include <vhl/sync/Semaphore.hpp>
#include <vhl/sync/Fence.hpp>

#include <unordered_map>
#include <stdexcept>

namespace tungol::vhl 
{
	struct Frame 
	{
		Frame(const Device& device, QueueFamily queueFamily)
		{
			commandPool = std::make_unique<CommandPool>(device, queueFamily, 1);
			
			commandBuffer = &commandPool->commandBuffer();
			
			fence = std::make_unique<Fence>(device);

			receivedImage = std::make_unique<Semaphore>(device);
		}
		
		Frame(Frame&& frame) noexcept : commandPool(std::move(frame.commandPool)),
			commandBuffer(frame.commandBuffer),
			fence(std::move(frame.fence)), receivedImage(std::move(frame.receivedImage))
		{}

		Frame& operator=(Frame&& frame) noexcept 
		{
			commandPool = std::move(frame.commandPool);
			
			commandBuffer = frame.commandBuffer;

			fence = std::move(frame.fence);
			receivedImage = std::move(frame.receivedImage);

			return *this;
		}

		bool needsReset = true;
		std::unique_ptr<CommandPool> commandPool;

		CommandBuffer* commandBuffer;

		std::unique_ptr<Fence> fence;

		std::unique_ptr<Semaphore> receivedImage;
	};

	
	struct CommandQueue::Impl
	{	
		void submitUniversal();

		AcquireResult nextImageIndex(const Swapchain& swapchain);
		
		void resetCmd();

		void begin();
		void submit();
		bool end(const Swapchain& swapchain, uint32_t imageIndex);

		CommandBuffer& cmd() const;

		uint32_t currentFrameIndex = 0, framesInFlight = 4;

		Queue* universalQueue = nullptr;
		QueueFamily queueFamily = QueueFamilyIgnored;

		std::optional<uint32_t> currentImageIndex;

		std::vector<Frame> frames;
		std::vector<std::unique_ptr<Semaphore>> presentImages;
	};

	CommandQueue::CommandQueue(Device& device, uint32_t framesInFlight, uint32_t swapchainImageCount)
		: m_impl(new Impl)
	{
		QueueFamilyList& queueFamilyList = device.queueFamilyList();

		Queue* universal = queueFamilyList.findFirst(QueueUsage::Transfer | QueueUsage::Compute | QueueUsage::Graphics | QueueUsage::Present);

		if (!universal)
		{
			throw std::runtime_error("[CommandQueue::CommandQueue] No QueueUsage::Transfer | QueueUsage::Compute | QueueUsage::Graphics | QueueUsage::Present Queue found!");
		}

		QueueFamily universalFamily = universal->details().family;

		m_impl->universalQueue = universal;
		m_impl->queueFamily = universalFamily;
		m_impl->framesInFlight = framesInFlight;
		m_impl->frames.reserve(framesInFlight);
		
		for (uint32_t i = 0; i < framesInFlight; ++i) 
		{
			m_impl->frames.emplace_back(device, universalFamily);
		}

		m_impl->presentImages.reserve(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; ++i)
			m_impl->presentImages.emplace_back(std::make_unique<Semaphore>(device));
	}

	CommandQueue::~CommandQueue()
	{
		m_impl->presentImages.clear();
	}

	void CommandQueue::resetCmd()
	{
		m_impl->resetCmd();
	}

	CommandBuffer& CommandQueue::cmd() const
	{
		return m_impl->cmd();
	}

	Queue& CommandQueue::native() const
	{
		return *m_impl->universalQueue;
	}

	void CommandQueue::begin()
	{
		m_impl->begin();
	}

	AcquireResult CommandQueue::nextImageIndex(const Swapchain& swapchain)
	{
		return m_impl->nextImageIndex(swapchain);
	}

	void CommandQueue::submit()
	{
		m_impl->submitUniversal();
	}

	bool CommandQueue::end(const Swapchain& swapchain, uint32_t imageIndex)
	{
		return m_impl->end(swapchain, imageIndex);
	}

	uint32_t CommandQueue::frameSlot() const
	{
		return m_impl->currentFrameIndex;
	}

	void CommandQueue::wait()
	{
		m_impl->universalQueue->wait();
	}
	void CommandQueue::Impl::submitUniversal()
	{
		const Frame& frame = frames[currentFrameIndex];

		SemaphoreSubmitInfo lastWait
		{
			.semaphore = frame.receivedImage.get(),
			.stage = PipelineStageFlags::ColorAttachmentOutput,
		};

		CommandBuffer* commandBuffer = frame.commandBuffer;

		if (!currentImageIndex.has_value())
		{
			SubmitInfo submitInfo{ .commandBufferCount = 1, .commandBuffers = commandBuffer, .fence = frame.fence.get() };
			commandBuffer->end();
			universalQueue->submit(submitInfo);
			return;
		}
 
		SemaphoreSubmitInfo lastSignal
		{
			.semaphore = presentImages[*currentImageIndex].get(),
			.stage = PipelineStageFlags::ColorAttachmentOutput
		};

		SubmitInfo submitInfo
		{
			.commandBufferCount = 1,
			.commandBuffers = commandBuffer,
			.waitCount = 1,
			.waits = &lastWait,
			.signalCount = 1,
			.signals = &lastSignal,
			.fence = frame.fence.get()
		};

		commandBuffer->end();

		universalQueue->submit(submitInfo);
	}

	void CommandQueue::Impl::begin()
	{
		if (currentImageIndex.has_value())
			throw std::runtime_error("[CommandScheduler::begin] begin() before end()");

		auto& frame = frames[currentFrameIndex];

		frame.fence->wait();
		frame.fence->reset();
	
		if (frame.needsReset) frame.commandPool->reset();
		frame.commandBuffer->begin();
		frame.needsReset = true;
	}

	
	AcquireResult CommandQueue::Impl::nextImageIndex(const Swapchain& swapchain)
	{
		auto& frame = frames[currentFrameIndex];

		AcquireInfo acquireInfo
		{
			.semaphore = frame.receivedImage.get()
		};

		AcquireResult result = swapchain.acquire(acquireInfo);
		
		if (result.imageIndex.has_value())
			currentImageIndex = result.imageIndex;
		else currentImageIndex = std::nullopt;

		return result;
	}

	void CommandQueue::Impl::resetCmd()
	{
		auto& frame = frames[currentFrameIndex];
		frame.commandPool->reset();
	}

	bool CommandQueue::Impl::end(const Swapchain& swapchain, uint32_t imageIndex)
	{
		if (imageIndex != currentImageIndex || !currentImageIndex.has_value()) 
		{
			throw std::runtime_error("[CommandScheduler::end] end image index mismatch");
		}

		auto& frame = frames[currentFrameIndex];

		PresentInfo presentInfo
		{
			.swapchain = &swapchain,
			.wait = presentImages[imageIndex].get(),
			.imageIndex = imageIndex
		};

		bool ok = universalQueue->present(presentInfo);
		currentFrameIndex = (currentFrameIndex + 1) % framesInFlight;
		currentImageIndex = std::nullopt;

		return ok;
	}
	CommandBuffer& CommandQueue::Impl::cmd() const
	{
		if (currentFrameIndex >= framesInFlight) throw std::runtime_error("[CommandQueue::cmd] .. ");
		if (CommandBuffer* commandBuffer = frames[currentFrameIndex].commandBuffer)
			return *commandBuffer;
		else throw std::runtime_error("[CommandQueue::cmd] Command buffer is nullptr");
	}
}



