#pragma once
#include <vhl/core/Device.hpp>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT CommandBuffer
	{
	public:
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		
		CommandBuffer(CommandBuffer&& other) noexcept = default;
		CommandBuffer& operator=(CommandBuffer&& other) noexcept = default;

		CommandBuffer(CommandBufferHandle hande, const CommandLimits& limits);

		CommandLimits limits() const { return m_limits; }

		void begin();
		void end();

		CommandBufferHandle handle() const;
	private:
		CommandBufferHandle m_handle{ .value = 0 };
		CommandLimits m_limits;
	};

	class TUNGOL_EXPORT CommandPool
	{
	public:
		// @param device The device used to create the command pool.
		// @param family The queue family index of the queue used during submission.
		// @param count The count of command buffer command pool can allocate.
		CommandPool(const Device& device, QueueFamily family, uint32_t count);
		~CommandPool();


		const CommandBuffer& commandBuffer(uint32_t index = 0) const;
		CommandBuffer& commandBuffer(uint32_t index = 0);
		
		uint32_t count() const;

		CommandLimits limits() const;

		void reset();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};


}
