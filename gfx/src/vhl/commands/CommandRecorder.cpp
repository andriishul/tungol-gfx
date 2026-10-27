#include "CommandRecorder.hpp"

#include "CommandBuffer.hpp"

#include <vhl/core/Descriptors.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/buffers/Buffer.hpp>
#include <vhl/core/images/ImageView.hpp>

#include <vhl/pipeline/Pipeline.hpp>

#include <vhl/utils/VkUtils.hpp>

namespace tungol::vhl 
{
	static VkRenderingAttachmentInfo toVkRenderingAttachmentInfo(const RenderingAttachment& attachment)
	{
		if (attachment.imageView)
		{
			if (uint64_t handle_value = attachment.imageView->handle().value; handle_value != 0)
			{
				uint64_t resolve_handle_value = 0;

				if (attachment.resolveImageView) resolve_handle_value = attachment.resolveImageView->handle().value;

				VkClearValue clearValue{};

				if (std::holds_alternative<std::array<float, 4>>(attachment.clearValue))
				{
					memcpy(clearValue.color.float32, std::get<std::array<float, 4>>(attachment.clearValue).data(), sizeof(float) * 4);
				}
				else if (std::holds_alternative<uint32_t>(attachment.clearValue))
				{
					clearValue.depthStencil.stencil = std::get<uint32_t>(attachment.clearValue);
				}
				else if (std::holds_alternative<float>(attachment.clearValue))
				{
					clearValue.depthStencil.depth = std::get<float>(attachment.clearValue);
				}
				else throw std::runtime_error("[RenderingAttachment::clearValue] no data specified.");

				return VkRenderingAttachmentInfo
				{
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = reinterpret_cast<VkImageView>(handle_value),
					.imageLayout = utils::toVkImageLayout(attachment.layout),
					
					.resolveMode = attachment.resolveImageView ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
					.resolveImageView = resolve_handle_value ? reinterpret_cast<VkImageView>(resolve_handle_value) : nullptr,
					.resolveImageLayout = resolve_handle_value ?  utils::toVkImageLayout(attachment.resolveLayout) : VK_IMAGE_LAYOUT_UNDEFINED,

					.loadOp = utils::toVkAttachmentLoadOp(attachment.loadOp),
					.storeOp = utils::toVkAttachmentStoreOp(attachment.storeOp),
					.clearValue = clearValue
				};
			}
			else throw std::runtime_error("[toVkRenderingAttachmentInfo] Null image view handle.");
		}

		return VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	}

	struct CommandRecorder::Impl 
	{
		CommandLimits limits{};
		VkCommandBuffer commandBuffer = nullptr;
	};


	CommandRecorder::CommandRecorder(const CommandBuffer& commandBuffer) : m_impl(new Impl)
	{
		if (uint64_t handle_value = commandBuffer.handle().value; handle_value != 0)
			m_impl->commandBuffer = reinterpret_cast<VkCommandBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::CommandRecorder] Null command buffer handle.");

		m_impl->limits = commandBuffer.limits();
	}

	CommandRecorder::~CommandRecorder() = default;

	void CommandRecorder::bindPipeline(const Pipeline& pipeline)
	{
		VkPipeline vkPipeline = VK_NULL_HANDLE;
		VkPipelineBindPoint vkBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		if (uint64_t handle_value = pipeline.handle().value; handle_value != 0)
			vkPipeline = reinterpret_cast<VkPipeline>(handle_value);
		else throw std::runtime_error("[CommandRecorder::bindPipeline] Null pipeline handle.");

		switch (pipeline.type())
		{
			case PipelineType::Graphics:
				vkBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				break;
			case PipelineType::Compute:
				vkBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
				break;
		}

			
		vkCmdBindPipeline(m_impl->commandBuffer, vkBindPoint, vkPipeline);
	}

	void CommandRecorder::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		vkCmdDispatch(m_impl->commandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void CommandRecorder::clearAttachment(ImageAspectFlags aspect, const Scissor& s, const std::variant<std::array<float, 4>, float, uint32_t>& clearValue)
	{
		VkClearAttachment attachment{};
		attachment.aspectMask = utils::toVkImageAspectFlags(aspect);

		if (std::holds_alternative<std::array<float, 4>>(clearValue))
		{
			memcpy(attachment.clearValue.color.float32, std::get<std::array<float, 4>>(clearValue).data(), sizeof(float) * 4);
		}
		else if (std::holds_alternative<uint32_t>(clearValue))
		{
			attachment.clearValue.depthStencil.stencil = std::get<uint32_t>(clearValue);
		}
		else if (std::holds_alternative<float>(clearValue))
		{
			attachment.clearValue.depthStencil.depth = std::get<float>(clearValue);
		}
		else throw std::runtime_error("[CommandRecorder::clearAttachment] no data specified.");

		VkClearRect rect{};
		rect.rect.offset = { s.offsetX, s.offsetY };
		rect.rect.extent = { s.width, s.height };
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;

		vkCmdClearAttachments(m_impl->commandBuffer, 1, &attachment, 1, &rect);
	}

	void CommandRecorder::beginRendering(const RenderingInfo& renderingInfo)
	{	
		VkRect2D vkRenderArea
		{
			.offset = {0, 0},
			.extent = {renderingInfo.width, renderingInfo.height}
		};

		VkRenderingAttachmentInfo colorAttachment = toVkRenderingAttachmentInfo(renderingInfo.colorAttachment);
		VkRenderingAttachmentInfo depthAttachment = toVkRenderingAttachmentInfo(renderingInfo.depthAttachment);
		VkRenderingAttachmentInfo stencilAttachment = toVkRenderingAttachmentInfo(renderingInfo.stencilAttachment);

		VkRenderingInfo vkRenderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = vkRenderArea,
			.layerCount = 1,
			 //.viewMask
			.colorAttachmentCount = colorAttachment.imageView ? 1U : 0U,
			.pColorAttachments = colorAttachment.imageView ? &colorAttachment : nullptr,
			.pDepthAttachment = depthAttachment.imageView ? &depthAttachment : nullptr,
			.pStencilAttachment = stencilAttachment.imageView ? &stencilAttachment : nullptr,
		};

		vkCmdBeginRendering(m_impl->commandBuffer, &vkRenderingInfo);
	}
	void CommandRecorder::endRendering()
	{
		vkCmdEndRendering(m_impl->commandBuffer);
	}

	void CommandRecorder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		vkCmdDraw(m_impl->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandRecorder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		vkCmdDrawIndexed(m_impl->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	uint32_t CommandRecorder::drawIndirect(const Buffer& buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
	{
		VkBuffer vkBuffer = 0;

		if (buffer.type() != BufferType::Indirect) throw std::runtime_error("[CommandRecorder::drawIndirect] Buffer is not INDIRECT.");

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::drawIndirect] Null buffer handle.");

		if (drawCount > m_impl->limits.maxDrawIndirectCount)
			drawCount = m_impl->limits.maxDrawIndirectCount;

		vkCmdDrawIndirect(m_impl->commandBuffer, vkBuffer, offset, drawCount, stride);
		
		return drawCount;
	}

	uint32_t CommandRecorder::drawIndexedIndirect(const Buffer& buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
	{
		VkBuffer vkBuffer = 0;

		if (buffer.type() != BufferType::Indirect) throw std::runtime_error("[CommandRecorder::drawIndexedIndirect] Buffer is not INDIRECT.");

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::drawIndexedIndirect] Null buffer handle.");

		if (drawCount > m_impl->limits.maxDrawIndirectCount)
			drawCount = m_impl->limits.maxDrawIndirectCount;

		vkCmdDrawIndexedIndirect(m_impl->commandBuffer, vkBuffer, offset, drawCount, stride);

		return drawCount;
	}

	void CommandRecorder::setScissor(const Scissor& scissor)
	{
		VkRect2D vkScissor = utils::toVkScissor(scissor);

		vkCmdSetScissor(m_impl->commandBuffer, 0, 1, &vkScissor);
	}
	
	void CommandRecorder::setViewport(const Viewport& viewport)
	{
		VkViewport vkScissor = utils::toVkViewport(viewport);

		vkCmdSetViewport(m_impl->commandBuffer, 0, 1, &vkScissor);
	}

	constexpr uint64_t MAX_UPDATE_BUFFER_SIZE = 65536U;

	void CommandRecorder::updateUniformBuffer(const Buffer& buffer, const void* pData, uint64_t offset, uint64_t size)
	{
		if (size > MAX_UPDATE_BUFFER_SIZE) throw std::runtime_error("[CommandRecorder::updateUniformBuffer] Size exceeds Vulkan's 64 KB limit for vkCmdUpdateBuffer.");
		if (buffer.type() != BufferType::Uniform) throw std::runtime_error("[CommandRecorder::updateUniformBuffer] Buffer type isnot UNIFORM.");
		if ((offset % 4) != 0 || (size % 4) != 0) throw std::runtime_error("[CommandRecorder::updateUniformBuffer] Offset and size must be multiples of 4.");
		if (offset + size > buffer.size()) throw std::runtime_error("[CommandRecorder::updateUniformBuffer] Update exceeds buffer bounds.");
		
		VkBuffer vkBuffer = VK_NULL_HANDLE;

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::updateUniformBuffer] Null uniform buffer handle.");
		
		vkCmdUpdateBuffer(m_impl->commandBuffer, vkBuffer, offset, size, pData);
	}

	void CommandRecorder::bindVertexBuffer(const Buffer& buffer, uint32_t firstBinding, uint64_t offset)
	{
		if (buffer.type() != BufferType::Vertex) throw std::runtime_error("[CommandRecorder::bindVertexBuffer] Buffer type isnot VERTEX.");

		VkBuffer vkBuffer = 0;

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::bindVertexBuffer] Null buffer handle.");

		vkCmdBindVertexBuffers(m_impl->commandBuffer, firstBinding, 1, &vkBuffer, &offset);
	}

	void CommandRecorder::bindIndexBuffer(const Buffer& buffer, IndexType type, uint64_t offset)
	{
		if (buffer.type() != BufferType::Index) throw std::runtime_error("[CommandRecorder::bindIndexBuffer] Buffer type isnot INDEX.");

		VkBuffer vkBuffer = 0;

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::bindIndexBuffer] Null buffer handle.");

		VkIndexType vkType = utils::toVkIndexType(type);

		vkCmdBindIndexBuffer(m_impl->commandBuffer, vkBuffer, offset, vkType);
	}

	void CommandRecorder::copyTo(const Buffer& staging, const Buffer& buffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		if(staging.type() != BufferType::Staging) throw std::runtime_error("[CommandRecorder::copyTo] Staging buffer type isnot STAGING.");

		VkBuffer vkStaging = 0;
		VkBuffer vkBuffer = 0;

		if (uint64_t handle_value = staging.handle().value; handle_value != 0)
			vkStaging = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::copyTo] Null staging buffer handle.");

		if (uint64_t handle_value = buffer.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::copyTo] Null buffer handle.");

		VkBufferCopy2 bufferCopy
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.srcOffset = srcOffset,
			.dstOffset = dstOffset,
			.size = size
		};

		VkCopyBufferInfo2 copyInfo
		{
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.srcBuffer = vkStaging,
			.dstBuffer = vkBuffer,
			.regionCount = 1,
			.pRegions = &bufferCopy
		};

		vkCmdCopyBuffer2(m_impl->commandBuffer, &copyInfo);
	}

	void CommandRecorder::copyTo(const Buffer& staging, const Image& image, uint64_t srcOffset, uint32_t width, uint32_t height, uint32_t depth, 
		int32_t dstX, int32_t dstY, int32_t dstZ, uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t layerCount)
	{
		if (staging.type() != BufferType::Staging) throw std::runtime_error("[CommandRecorder::bindVertexBuffer] Staging buffer type isnot STAGING.");

		VkBuffer vkStaging = 0;
		VkImage vkImage = 0;

		if (uint64_t handle_value = staging.handle().value; handle_value != 0)
			vkStaging = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[CommandRecorder::copyTo] Null staging buffer handle.");

		if (uint64_t handle_value = image.handle().value; handle_value != 0)
			vkImage = reinterpret_cast<VkImage>(handle_value);
		else throw std::runtime_error("[CommandRecorder::copyTo] Null image handle.");

		VkImageSubresourceLayers subresource
		{
			.aspectMask = utils::toVkImageAspectFlags(aspectOf(image)),
			.mipLevel = mipLevel,
			.baseArrayLayer = baseArrayLayer,
			.layerCount = layerCount
		};


		VkBufferImageCopy2 imageCopy
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.bufferOffset = srcOffset,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = subresource,
			.imageOffset = { dstX, dstY, dstZ },
			.imageExtent = { width, height, depth }
		};


		VkCopyBufferToImageInfo2 copyInfo
		{
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			.srcBuffer = vkStaging,
			.dstImage = vkImage,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = 1,
			.pRegions = &imageCopy
		};


		vkCmdCopyBufferToImage2(m_impl->commandBuffer, &copyInfo);
	}

	void CommandRecorder::bindDescriptorSet(const DescriptorSetHandle& handle, PipelineType type, PipelineLayoutHandle layout, uint32_t firstSet, const std::vector<uint32_t>& dynamicOffsets)
	{
		VkDescriptorSet vkSet = 0;
		VkPipelineLayout vkLayout = 0;

		if (uint64_t handle_value = layout.value; handle_value != 0)
			vkLayout = reinterpret_cast<VkPipelineLayout>(handle_value);
		else throw std::runtime_error("[CommandRecorder::bindDescriptorSet] Null pipeline layout handle.");

		if (uint64_t handle_value = handle.value; handle_value != 0)
			vkSet = reinterpret_cast<VkDescriptorSet>(handle_value);
		else throw std::runtime_error("[CommandRecorder::bindDescriptorSet] Null descriptor set handle.");


		VkPipelineBindPoint vkBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		if (type == PipelineType::Compute) vkBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		vkCmdBindDescriptorSets(m_impl->commandBuffer, vkBindPoint, vkLayout, firstSet, 1, &vkSet,
			static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.empty() ? nullptr : dynamicOffsets.data());
	}

	void CommandRecorder::pushConstants(PipelineLayoutHandle layout, ShaderStageFlags stage, uint32_t offset, uint32_t size, const void* pValues)
	{
		VkShaderStageFlags vkStage = utils::toVkShaderStageFlags(stage);
		VkPipelineLayout vkLayout = 0;

		if (uint64_t handle_value = layout.value; handle_value != 0)
			vkLayout = reinterpret_cast<VkPipelineLayout>(handle_value);
		else throw std::runtime_error("[CommandRecorder::pushConstants] Null pipeline layout handle.");

		vkCmdPushConstants(m_impl->commandBuffer, vkLayout, vkStage, offset, size, pValues);
	}

}
