#include "Fence.hpp"

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{
	struct Fence::Impl 
	{
		VkDevice logical = VK_NULL_HANDLE;

		VkFence fence = VK_NULL_HANDLE;
	};

	Fence::Fence(const Device& device) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
		{
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		}
		else throw std::runtime_error("[Fence::Fence] Null device handle");


		VkFenceCreateInfo fenceInfo
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		VK_CHECK(vkCreateFence(m_impl->logical, &fenceInfo, nullptr, &m_impl->fence));
	}

	Fence::~Fence() 
	{
		if(m_impl && m_impl->fence && m_impl->logical)
			vkDestroyFence(m_impl->logical, m_impl->fence, nullptr);
	}

	bool Fence::ready() const
	{
		VkResult res = vkGetFenceStatus(m_impl->logical, m_impl->fence);
		VK_CHECK(res);
		return res == VK_SUCCESS;
	}

	void Fence::wait()
	{
		VK_CHECK(vkWaitForFences(m_impl->logical, 1, &m_impl->fence, VK_TRUE, UINT64_MAX));
	}
	void Fence::reset()
	{
		VK_CHECK(vkResetFences(m_impl->logical, 1, &m_impl->fence));
	}
	FenceHandle Fence::handle() const
	{
		return FenceHandle
		{
			.value = reinterpret_cast<uint64_t>(m_impl->fence)
		};
	}
}
