#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <vhl/core/GraphicsCommon.hpp>

#include <vector>
#include <array>
#include <variant>

namespace tungol::vhl 
{
	// DYNAMIC RENDERING

	struct RenderingAttachment
	{
		ImageView* imageView;
		ImageLayout layout;

		ImageView* resolveImageView = nullptr;
		ImageLayout resolveLayout = ImageLayout::Undefined;

		AttachmentLoadOp loadOp;
		AttachmentStoreOp storeOp;

		// COLOR, DEPTH, STENCIL
		std::variant<std::array<float, 4>, float, uint32_t> clearValue;
	};

	struct RenderingInfo
	{
		uint32_t width = 0, height = 0;

		RenderingAttachment colorAttachment;
		RenderingAttachment depthAttachment;
		RenderingAttachment stencilAttachment;
	};


	class TUNGOL_EXPORT CommandRecorder 
	{
	public:
		explicit CommandRecorder(const CommandBuffer& commandBuffer);
		~CommandRecorder();

		void bindPipeline(const Pipeline& pipeline);

		void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void clearAttachment(ImageAspectFlags layout, const Scissor& s, const std::variant<std::array<float, 4>, float, uint32_t>& clearValue);

		void beginRendering(const RenderingInfo&);
		void endRendering();

		void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
		
		// Returns the number of indirect draws recorded (clamped to device limit).
		uint32_t drawIndirect(const Buffer& buffer, uint64_t offset, uint32_t drawCount, uint32_t stride);
		// Returns the number of indexed indirect draws recorded (clamped to device limit).
		uint32_t drawIndexedIndirect(const Buffer& buffer, uint64_t offset, uint32_t drawCount, uint32_t stride);

		void setViewport(const Viewport&);
		void setScissor(const Scissor&);

		void updateUniformBuffer(const Buffer& buffer, const void* pData, uint64_t offset, uint64_t size);

		void bindVertexBuffer(const Buffer& buffer, uint32_t firstBinding = 0, uint64_t offset = 0);
		void bindIndexBuffer(const Buffer& buffer, IndexType type, uint64_t offset = 0);

		void copyTo(const Buffer& staging, const Buffer& buffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size);
		void copyTo(const Buffer& staging, const Image& image, uint64_t srcOffset, uint32_t width, uint32_t height, uint32_t depth = 1, int32_t dstX = 0, int32_t dstY = 0, int32_t dstZ = 0, uint32_t mipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);

		void bindDescriptorSet(const DescriptorSetHandle& set, PipelineType type, PipelineLayoutHandle layout, uint32_t firstSet, const std::vector<uint32_t>& dynamicOffsets = {});
	
		void pushConstants(PipelineLayoutHandle layout, ShaderStageFlags stage, uint32_t offset, uint32_t size, const void* pValues);
	private:
		struct Impl; 
		std::unique_ptr<Impl> m_impl;
	};
}
