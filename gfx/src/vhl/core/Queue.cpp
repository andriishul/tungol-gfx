#include "Queue.hpp"

#include <vhl/commands/CommandBuffer.hpp>

#include <vhl/core/Swapchain.hpp>

#include <vhl/sync/Semaphore.hpp>
#include <vhl/sync/Fence.hpp>

#include <vhl/utils/VkInspector.hpp>


namespace tungol::vhl 
{
	static std::vector<VkSemaphoreSubmitInfo> toVectorVkSemaphoreSubmitInfo(const SemaphoreSubmitInfo* submits, uint32_t count)
	{
		if (!submits || !count) return std::vector<VkSemaphoreSubmitInfo>();

		std::vector<VkSemaphoreSubmitInfo> vkSubmits(count);

		for (uint32_t i = 0; i < count; ++i)
		{
			VkSemaphore vkSemaphore = 0;

			const Semaphore* semaphore = submits[i].semaphore;

			if (semaphore) 
			{
				if (uint64_t handle_value = semaphore->handle().value; handle_value != 0)
					vkSemaphore = reinterpret_cast<VkSemaphore>(handle_value);
				else throw std::runtime_error("[toVectorVkSemaphoreSubmitInfo] Null semaphore handle");
			}
			VkSemaphoreSubmitInfo submitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = vkSemaphore,
				.value = submits[i].value,
				.stageMask = utils::toVkPipelineStageFlags2(submits[i].stage)
			};

			vkSubmits[i] = submitInfo;
		}

		return vkSubmits;
	}


	struct Queue::Impl 
	{
		VkQueue queue = VK_NULL_HANDLE;

		QueueFamilyDetails queueFamilyDetails{};
	};

	const QueueFamilyDetails& Queue::details() const
	{
		return m_impl->queueFamilyDetails;
	}

	void Queue::wait()
	{
		VK_CHECK(vkQueueWaitIdle(m_impl->queue));
	}

	void Queue::submit(const SubmitInfo& submitInfo)
	{
		VkFence vkFence = VK_NULL_HANDLE;
		
		auto vkSignals = toVectorVkSemaphoreSubmitInfo(submitInfo.signals, submitInfo.signalCount);
		auto vkWaits = toVectorVkSemaphoreSubmitInfo(submitInfo.waits, submitInfo.waitCount);


		if (submitInfo.commandBufferCount > 0 && !submitInfo.commandBuffers)
			throw std::runtime_error("[Queue::submit] Null commandBuffers");


		if (submitInfo.fence)
		{

			if (uint64_t handle_value = submitInfo.fence->handle().value; handle_value != 0)
			{
				vkFence = reinterpret_cast<VkFence>(handle_value);
			}
			else
			{
				throw std::runtime_error("[Queue::submit] Null fence handle");
			}
		}
		
		std::vector<VkCommandBufferSubmitInfo> cmdBufferSubmitInfos(submitInfo.commandBufferCount);

		for (uint32_t i = 0; i < submitInfo.commandBufferCount; ++i)
		{
			cmdBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmdBufferSubmitInfos[i].pNext = nullptr;
			cmdBufferSubmitInfos[i].deviceMask = 0;

			if (uint64_t handle_value = submitInfo.commandBuffers[i].handle().value; handle_value != 0)
			{
				cmdBufferSubmitInfos[i].commandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);
			}
			else throw std::runtime_error("[Queue::submit] Null command buffer handle");
		}

		uint32_t waitSemaphoreInfoCount = submitInfo.waits ? static_cast<uint32_t>(vkWaits.size()) : 0;
		uint32_t signalSemaphoreInfoCount = submitInfo.signals ? static_cast<uint32_t>(vkSignals.size()) : 0;

		VkSubmitInfo2 vkSubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = waitSemaphoreInfoCount,
			.pWaitSemaphoreInfos = waitSemaphoreInfoCount ? vkWaits.data() : nullptr,

			.commandBufferInfoCount = submitInfo.commandBufferCount,
			.pCommandBufferInfos = cmdBufferSubmitInfos.data(),

			.signalSemaphoreInfoCount = signalSemaphoreInfoCount,
			.pSignalSemaphoreInfos = signalSemaphoreInfoCount ? vkSignals.data() : nullptr,
		};

		VK_CHECK(vkQueueSubmit2(m_impl->queue, 1, &vkSubmitInfo, vkFence));
	}

	bool Queue::present(const PresentInfo& presentInfo)
	{
		if (!details().present)
			throw std::runtime_error("[Queue::present] This queue family doesnot support presentation!");

		VkSemaphore vkWait = VK_NULL_HANDLE;
		VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE;

		uint32_t swapChainCount = 0;
		uint32_t waitCount = 0;


		if (presentInfo.wait)
		{
			if (uint64_t handle_value = presentInfo.wait->handle().value; handle_value != 0)
			{
				vkWait = reinterpret_cast<VkSemaphore>(handle_value);
				waitCount++;
			}
		}
		if (presentInfo.swapchain)
		{
			if (uint64_t handle_value = presentInfo.swapchain->handle().value; handle_value != 0)
			{
				vkSwapchain = reinterpret_cast<VkSwapchainKHR>(handle_value);
				swapChainCount++;
			}
			else throw std::runtime_error("[Queue::present] Null swapchain handle");
		}

		VkPresentInfoKHR vkPresentInfo
		{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = waitCount,
			.pWaitSemaphores = waitCount ? &vkWait : nullptr,
			.swapchainCount = swapChainCount,
			.pSwapchains = swapChainCount ? &vkSwapchain : nullptr,
			.pImageIndices = &presentInfo.imageIndex
		};
		
		VkResult vkRes = vkQueuePresentKHR(m_impl->queue, &vkPresentInfo);

		if (vkRes == VK_ERROR_OUT_OF_DATE_KHR || vkRes == VK_SUBOPTIMAL_KHR)
			return false;

		VK_CHECK(vkRes);

		return true;
	}

	Queue::Queue(const Device& device,
		const QueueFamilyDetails& details,
		uint32_t index) : m_impl(new Impl)
	{
		m_impl->queueFamilyDetails = details;

		VkDeviceQueueInfo2 queueInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
			.queueFamilyIndex = details.family,
			.queueIndex = index
		};
		
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
		{
			VkDevice logical = reinterpret_cast<VkDevice>(handle_value);

			vkGetDeviceQueue2(logical, &queueInfo, &m_impl->queue);
		}
		else throw std::runtime_error("[Queue::Queue] Null device handle");
	}

	Queue::Queue(Queue&& queue) noexcept : m_impl(std::move(queue.m_impl))
	{
	}

	Queue::~Queue() = default;

	Queue& Queue::operator=(Queue&& queue) noexcept
	{
		m_impl = std::move(queue.m_impl);
		return *this;
	}
	
	struct QueueEntry 
	{
		QueueUsage usage = QueueUsage::None;

		QueueFamilyDetails familyDetails;

		std::vector<Queue> queues;
	};


	struct QueueFamilyList::Impl
	{
		const Device& device;
		std::vector<QueueEntry> entries;
	};

	QueueFamilyList::QueueFamilyList(const Device& device, const std::vector<QueueFamilyDetails>& details) : m_impl(new Impl{device})
	{
		for (const auto& queueDetails : details)
		{
			QueueUsage usage = toQueueUsage(queueDetails);
			std::vector<Queue> queues;
			queues.reserve(queueDetails.count);
			queues.emplace_back(device, queueDetails, uint32_t(0));
			m_impl->entries.push_back(QueueEntry{ usage, queueDetails, std::move(queues) });
		}
	}

	QueueFamilyList ::~QueueFamilyList() = default;

	using namespace utils;

	Queue* QueueFamilyList::findFirst(QueueUsage usage, QueueFamily family)
	{
		for (uint32_t i = 0; i < count(); ++i)
		{
			auto& entry = m_impl->entries[i];

			if ((usage & entry.usage) != QueueUsage::None && family != entry.familyDetails.family)
			{
				return &entry.queues.front();
			}
		}

		return nullptr;
	}

	const Queue* QueueFamilyList::findFirst(QueueUsage usage, QueueFamily family) const
	{
		for (uint32_t i = 0; i < count(); ++i)
		{
			auto& entry = m_impl->entries[i];

			if ((usage & entry.usage) != QueueUsage::None && family != entry.familyDetails.family)
			{
				return &entry.queues.front();
			}
		}
		
		return nullptr;
	}

	Queue* QueueFamilyList::insert(QueueFamily family)
	{
		for (auto& entry : m_impl->entries)
		{
			if (entry.familyDetails.family == family) 
			{
				uint32_t queueIndex = static_cast<uint32_t>(entry.queues.size());

				if (queueIndex < entry.familyDetails.count)
				{
					return &entry.queues.emplace_back(m_impl->device, entry.familyDetails, queueIndex);
				}
				else throw std::runtime_error("[QueueFamilyList::insert] Reached limit of creating count of this queue family: " + std::to_string(family));
			}
		}

		throw std::runtime_error("[QueueFamilyList::insert] Queue family not found: " + std::to_string(family));
	}

	uint32_t QueueFamilyList::size(QueueFamily family) const
	{

		for (auto& entry : m_impl->entries)
		{
			if (entry.familyDetails.family == family) return static_cast<uint32_t>(entry.queues.size());
		}

		return 0;
	}

	uint32_t QueueFamilyList::count() const
	{
		return static_cast<uint32_t>(m_impl->entries.size());
	}

}
