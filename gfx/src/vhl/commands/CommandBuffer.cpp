#include "CommandBuffer.hpp"

#include "vhl/utils/VkInspector.hpp"

namespace tungol::vhl 
{
	CommandBuffer::CommandBuffer(CommandBufferHandle handle, const CommandLimits& limits) 
		: m_handle(handle), m_limits(limits) {}

	void CommandBuffer::begin()
	{
		if (uint64_t handle_value = m_handle.value; handle_value != 0)
		{
			VkCommandBufferBeginInfo beginInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
			};

			VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);


			vkBeginCommandBuffer(commandBuffer, &beginInfo);
		}
		else throw std::runtime_error("[CommandBuffer::begin] Null command buffer handle.");
	}
	void CommandBuffer::end()
	{
		if (uint64_t handle_value = m_handle.value; handle_value != 0)
		{
			VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);

			vkEndCommandBuffer(commandBuffer);
		}
		else throw std::runtime_error("[CommandBuffer::end] Null command buffer handle.");
	}

	CommandBufferHandle CommandBuffer::handle() const
	{
		return m_handle;
	}

	struct CommandPool::Impl 
	{
		CommandLimits limits;

		VkDevice logical = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;

		std::vector<CommandBuffer> commandBuffers;
	};

	CommandPool::CommandPool(const Device& device, QueueFamily family, uint32_t count) : m_impl(new Impl{ device.limits().command })
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
		{
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);

			VkCommandPoolCreateInfo poolInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = 0,
				.queueFamilyIndex = family
			};

			VK_CHECK(vkCreateCommandPool(m_impl->logical, &poolInfo, nullptr, &m_impl->commandPool));

			std::vector<VkCommandBuffer> vkCommandBuffers(count);

			VkCommandBufferAllocateInfo commandBufferAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = m_impl->commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = count
			};
			try 
			{
				VK_CHECK(vkAllocateCommandBuffers(m_impl->logical, &commandBufferAllocInfo, vkCommandBuffers.data()));
			}
			catch (...) 
			{
				if (m_impl->commandPool) 
				{
					vkDestroyCommandPool(m_impl->logical, m_impl->commandPool, nullptr);
					m_impl->commandPool = nullptr;
				}

				throw;
			}
			for (const auto& vkCommandBuffer : vkCommandBuffers)
				m_impl->commandBuffers.emplace_back(CommandBufferHandle{ reinterpret_cast<uintptr_t>(vkCommandBuffer) }, limits());
		}
		else throw std::runtime_error("[CommandPool::CommandPool] Null device handle.");
	}
	CommandPool::~CommandPool()
	{
		if(m_impl && m_impl->logical && m_impl->commandPool)
			vkDestroyCommandPool(m_impl->logical, m_impl->commandPool, nullptr);
	}

	const CommandBuffer& CommandPool::commandBuffer(uint32_t index) const
	{
		if (count() <= index)
		{
			std::ostringstream oss;
			oss << "Command buffer out of range: " << count()
				<< " acessing: " << index;

			throw std::out_of_range(oss.str());
		}

		return m_impl->commandBuffers[index];
	}

	CommandBuffer& CommandPool::commandBuffer(uint32_t index)
	{
		if (count() <= index)
		{
			std::ostringstream oss;
			oss << "Command buffer out of range: " << count()
				<< " acessing: " << index;

			throw std::out_of_range(oss.str());
		}

		return m_impl->commandBuffers[index];
	}
	

	CommandLimits CommandPool::limits() const
	{
		return m_impl->limits;
	}

	uint32_t CommandPool::count() const
	{
		return static_cast<uint32_t>(m_impl->commandBuffers.size());
	}
	void CommandPool::reset()
	{
		VK_CHECK(vkResetCommandPool(m_impl->logical, m_impl->commandPool, 0));
	}
}
