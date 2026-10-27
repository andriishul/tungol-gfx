#pragma once
#include "GraphicsCommon.hpp"

#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <vector>
#include <memory>      

namespace tungol::vhl
{
	struct SemaphoreSubmitInfo
	{
		const Semaphore* semaphore = nullptr;
		uint64_t value = 0; 
		PipelineStageFlags stage;
	};


	struct SubmitInfo
	{
		uint32_t commandBufferCount = 0;
		const CommandBuffer* commandBuffers = nullptr;

		uint32_t waitCount = 0;
		const SemaphoreSubmitInfo* waits = nullptr;
		uint32_t signalCount = 0;
		const SemaphoreSubmitInfo* signals = nullptr;

		const Fence* fence = nullptr;
	};

	
	struct PresentInfo
	{
		const Swapchain* swapchain = nullptr;
		const Semaphore* wait = nullptr;

		uint32_t imageIndex = 0;
	};

	class TUNGOL_EXPORT Queue 
	{
	public:
		
		/*
			Creates a queue from the specified queue family. 
			The queue index must be less than QueueFamilyDetails::count.
		*/
		Queue(const Device& device, const QueueFamilyDetails& details, uint32_t index);
		Queue(Queue&&) noexcept;
		~Queue();

		Queue& operator=(Queue&&) noexcept;

		const QueueFamilyDetails& details() const;

		void wait();
		
		void submit(const SubmitInfo& info);
		
		// Returns true when frame presented, swapchain healthy.
		bool present(const PresentInfo& info);
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	class TUNGOL_EXPORT QueueFamilyList
	{
	public:
		QueueFamilyList(const Device& device, const std::vector<QueueFamilyDetails>& details);
		~QueueFamilyList();

		// Find a queue with the specified usage bits, ignoring the given queue family.
		Queue* findFirst(QueueUsage usage, QueueFamily family = QueueFamilyIgnored);

		// Find a queue with the specified usage bits, ignoring the given queue family.
		const Queue* findFirst(QueueUsage usage, QueueFamily family = QueueFamilyIgnored) const;

		// Adds another queue to the specified family. Returns nullptr if family not found. Throws an error
		Queue* insert(QueueFamily family);

		// Returns the number of queues created for the given family.
		uint32_t size(QueueFamily family) const;
		
		// Returns the number of queue families.
		uint32_t count() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

}

