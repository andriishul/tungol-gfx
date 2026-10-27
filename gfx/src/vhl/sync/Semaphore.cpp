#include "Semaphore.hpp"

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{
	struct Semaphore::Impl 
	{
		VkDevice logical = VK_NULL_HANDLE;

		VkSemaphore semaphore = VK_NULL_HANDLE;
	};

	
	Semaphore::Semaphore(const Device& device) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
		{
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		}
		else throw std::runtime_error("[Semaphore::Semaphore] Null device handle");

		VkSemaphoreCreateInfo semaphoreInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};

		VK_CHECK(vkCreateSemaphore(m_impl->logical, &semaphoreInfo, nullptr, &m_impl->semaphore));
	}
	Semaphore::~Semaphore()
	{
		if(m_impl && m_impl->logical && m_impl->semaphore)
			vkDestroySemaphore(m_impl->logical, m_impl->semaphore, nullptr);
	}
	
	SemaphoreHandle Semaphore::handle() const
	{
		return SemaphoreHandle
		{
			.value = reinterpret_cast<uint64_t>(m_impl->semaphore)
		};
	}
}
