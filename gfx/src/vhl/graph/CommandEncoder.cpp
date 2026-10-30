#include "CommandEncoder.hpp"

#include "HazardTracker.hpp"
#include <vhl/commands/BarrierCommand.hpp>

#include <stdexcept>
#include <unordered_map>

namespace tungol::vhl 
{
	struct ImageBarrierOp
	{
		SubresourceState source;
		uint32_t baseMip, mipCount;
		uint32_t baseLayer, layerCount;
	};
	
	using BufferRangeBarrierOp = BufferRangeState;

	static bool sameBarrierSource(const SubresourceState& a, const SubresourceState& b)
	{
		return a.access == b.access && a.layout == b.layout && a.stage == b.stage;
	}

	static bool sameBarrierSource(const BufferRangeState& a, const BufferRangeState& b)
	{
		return a.access == b.access && a.stage == b.stage;
	}

	static std::vector<ImageBarrierOp> merge(const ImageState& state, uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount)
	{
		std::vector<ImageBarrierOp> ops;

		for (uint32_t mip = baseMip; mip < baseMip + mipCount; ++mip)
		{
			const auto& ref = state.at(mip, baseLayer);
			bool uniformMip = true;

			for (uint32_t layer = baseLayer + 1; layer < baseLayer + layerCount; ++layer)
			{
				if (!sameBarrierSource(ref, state.at(mip, layer)))
				{
					uniformMip = false;
					break;
				}
			}

			if (uniformMip)
			{
				const bool canExtend =
					!ops.empty()
					&& ops.back().baseLayer == baseLayer
					&& ops.back().layerCount == layerCount
					&& ops.back().baseMip + ops.back().mipCount == mip
					&& sameBarrierSource(ops.back().source, ref);

				if (canExtend) ++ops.back().mipCount;
				else ops.push_back({ ref, mip, 1, baseLayer, layerCount });
			}
			else
			{
				for (uint32_t layer = baseLayer; layer < baseLayer + layerCount; ++layer)
					ops.push_back({ state.at(mip, layer), mip, 1, layer, 1 });
			}
		}

		return ops;
	}

	static std::vector<BufferRangeBarrierOp> merge(const BufferState& state, uint64_t offset, uint64_t size) 
	{
		std::vector<BufferRangeBarrierOp> ops;

		const uint64_t end = offset + size;

		for (const auto& range : state.ranges)          
		{
			const uint64_t rEnd = range.offset + range.size;

			if (rEnd <= offset) continue;         
			if (range.offset >= end) continue;

			BufferRangeBarrierOp clipped = range;
			clipped.offset = std::max(range.offset, offset);
			clipped.size = std::min(rEnd, end) - clipped.offset;

			if (!ops.empty())
			{
				auto& last = ops.back();
				if (last.offset + last.size == clipped.offset &&
					last.access == clipped.access && last.stage == clipped.stage)
				{
					last.size += clipped.size;    
					continue;
				}
			}
			ops.push_back(clipped);
		}

		return ops;
	}

	struct SubresourceUsage 
	{
		UsageIntent intent = UsageIntent::None;

		uint32_t baseMip = 0;
		uint32_t mipCount = RemainingMipLevels;
		uint32_t baseLayer = 0;
		uint32_t layerCount = RemainingArrayLayers;
	};

	struct BufferRangeUsage 
	{
		UsageIntent intent = UsageIntent::None;

		uint64_t offset = 0;
		uint64_t size = BufferWholeSize;
	};

	struct Pass
	{
		std::function<void(CommandRecorder&)> callback;
		
		std::unordered_map<Image*, std::vector<SubresourceUsage>> imageUsages;
		std::unordered_map<Buffer*, std::vector<BufferRangeUsage>> bufferUsages;
	};

	CommandEncoder::PassBuilder::PassBuilder(CommandEncoder* encoder, uint32_t index) :
		m_encoder(encoder), m_index(index){}

	struct ImageTarget
	{
		ImageLayout layout = ImageLayout::Undefined;
		AccessFlags access = AccessFlags::None;
		PipelineStageFlags stage = PipelineStageFlags::None;
	};

	struct BufferTarget
	{
		AccessFlags access = AccessFlags::None;
		PipelineStageFlags stage = PipelineStageFlags::None;
	};

	static void commitState(ImageState& state, const ImageTarget& target, uint32_t mipBegin, uint32_t mipEnd, uint32_t layerBegin, uint32_t layerEnd)
	{
		for (uint32_t mip = mipBegin; mip < mipEnd; ++mip)
		{
			for (uint32_t layer = layerBegin; layer < layerEnd; ++layer)
			{
				auto& sr = state.at(mip, layer);
				sr.access = target.access;
				sr.stage = target.stage;
				sr.layout = target.layout;
			}
		}
	}
	
	static void commitRange(BufferState& state, const BufferTarget& t, uint64_t offset, uint64_t size)
	{
		for (auto& range : state.ranges) 
		{
			if (range.offset >= offset && range.offset + range.size <= offset + size)
			{
				range.access = t.access;
				range.stage = t.stage;
			}
		}
	}

	static ImageTarget resolveImage(UsageIntent usage)
	{
		ImageTarget target{};

		if ((usage & UsageIntent::TransferSrc) != UsageIntent::None)
		{
			target.layout = ImageLayout::TransferSrc;
			target.access = target.access | AccessFlags::TransferRead;
			target.stage = target.stage | PipelineStageFlags::Transfer;
		}
		if ((usage & UsageIntent::TransferDst) != UsageIntent::None)
		{
			target.layout = ImageLayout::TransferDst;
			target.access = target.access | AccessFlags::TransferWrite;
			target.stage = target.stage | PipelineStageFlags::Transfer;
		}
		
		if ((usage & UsageIntent::ShaderReadVertex) != UsageIntent::None)
		{
			target.layout = ImageLayout::ShaderReadOnly;
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::VertexShader;
		}
		if ((usage & UsageIntent::ShaderReadFragment) != UsageIntent::None)
		{
			target.layout = ImageLayout::ShaderReadOnly;
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::FragmentShader;
		}
		if ((usage & UsageIntent::ShaderReadCompute) != UsageIntent::None)
		{
			target.layout = ImageLayout::ShaderReadOnly;
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::ComputeShader;
		}

		if ((usage & UsageIntent::ShaderWriteVertex) != UsageIntent::None)
		{
			target.layout = ImageLayout::General;
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::VertexShader;
		}
		if ((usage & UsageIntent::ShaderWriteFragment) != UsageIntent::None)
		{
			target.layout = ImageLayout::General;
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::FragmentShader;
		}
		if ((usage & UsageIntent::ShaderWriteCompute) != UsageIntent::None)
		{
			target.layout = ImageLayout::General;
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::ComputeShader;
		}

		if ((usage & UsageIntent::ColorAttachment) != UsageIntent::None)
		{
			target.layout = ImageLayout::ColorAttachment;
			target.access = target.access | (AccessFlags::ColorAttachmentRead | AccessFlags::ColorAttachmentWrite);
			target.stage = target.stage | PipelineStageFlags::ColorAttachmentOutput;
		}
		if ((usage & UsageIntent::DepthStencilAttachment) != UsageIntent::None)
		{
			target.layout = ImageLayout::DepthStencilAttachment;
			target.access = target.access | (AccessFlags::DepthStencilRead | AccessFlags::DepthStencilWrite);
			target.stage = target.stage | (PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests);
		}
		if ((usage & UsageIntent::Present) != UsageIntent::None)
		{
			target.layout = ImageLayout::PresentSrc;
			target.stage = target.stage | PipelineStageFlags::ColorAttachmentOutput;
			target.access = target.access | AccessFlags::None;
		}

		return target;
	}

	static BufferTarget resolveBuffer(UsageIntent usage) 
	{
		BufferTarget target{};

		if ((usage & UsageIntent::TransferSrc) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::TransferRead;
			target.stage = target.stage | PipelineStageFlags::Transfer;
		}
		if ((usage & UsageIntent::TransferDst) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::TransferWrite;
			target.stage = target.stage | PipelineStageFlags::Transfer;
		}

		if ((usage & UsageIntent::ShaderReadVertex) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::VertexShader;
		}
		if ((usage & UsageIntent::ShaderReadFragment) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::FragmentShader;
		}
		if ((usage & UsageIntent::ShaderReadCompute) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderRead;
			target.stage = target.stage | PipelineStageFlags::ComputeShader;
		}

		if ((usage & UsageIntent::ShaderWriteVertex) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::VertexShader;
		}
		if ((usage & UsageIntent::ShaderWriteFragment) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::FragmentShader;
		}
		if ((usage & UsageIntent::ShaderWriteCompute) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::ShaderWrite;
			target.stage = target.stage | PipelineStageFlags::ComputeShader;
		}

		if ((usage & UsageIntent::VertexAttributeRead) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::VertexAttributeRead;
			target.stage = target.stage | PipelineStageFlags::VertexAttributeInput;
		}
		if ((usage & UsageIntent::IndexRead) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::IndexRead;
			target.stage = target.stage | PipelineStageFlags::IndexInput;
		}
		if ((usage & UsageIntent::IndirectRead) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::IndirectCommandRead;
			target.stage = target.stage | PipelineStageFlags::DrawIndirect;
		}


		if ((usage & UsageIntent::UniformReadFragment) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::UniformRead;
			target.stage = target.stage | PipelineStageFlags::FragmentShader;
		}
		if ((usage & UsageIntent::UniformReadCompute) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::UniformRead;
			target.stage = target.stage | PipelineStageFlags::ComputeShader;
		}
		if ((usage & UsageIntent::UniformReadVertex) != UsageIntent::None)
		{
			target.access = target.access | AccessFlags::UniformRead;
			target.stage = target.stage | PipelineStageFlags::VertexShader;
		}

		return target;
	} 

	struct CommandEncoder::Impl
	{
		CommandQueue& queue;
		HazardTracker& stateRegistry;

		Image* presentImage = nullptr;

		std::vector<Pass> passes;

		void transitionPresent();
		
		void transitionSubresources(const CommandBuffer& cmd, Image& img, const std::vector<ImageBarrierOp>& ops, 
			const ImageTarget& target, ImageState& state);

		void transitionImage(const CommandBuffer& cmd, Image& img, const ImageTarget& target, ImageState& state, 
			uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount);

		void transitionBufferRanges(const CommandBuffer& dstCmd, Buffer& buffer, const std::vector<BufferRangeBarrierOp>& ops,
			const BufferTarget& target, BufferState& state);

		void transitionBuffer(const CommandBuffer& dstCmd, Buffer& buffer, const BufferTarget& target, BufferState& state, uint64_t offset, uint64_t size);
	};

	
	void CommandEncoder::Impl::transitionImage(
		const CommandBuffer& dstCmd, Image& img, const ImageTarget& target, ImageState& state,
		uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount)
	{
		if (mipCount == RemainingMipLevels) mipCount = img.levelCount() - baseMip;
		if (layerCount == RemainingArrayLayers) layerCount = img.layerCount() - baseLayer;

		auto ops = merge(state, baseMip, mipCount, baseLayer, layerCount);

		transitionSubresources(dstCmd, img, ops, target, state);
	}

	void CommandEncoder::Impl::transitionBufferRanges(const CommandBuffer& dstCmd, Buffer& buffer, 
		const std::vector<BufferRangeBarrierOp>& ops, const BufferTarget& target, BufferState& state)
	{
		for (auto& op : ops) 
		{
			const bool same = op.access == target.access && op.stage == target.stage 
				&& ((target.access & AccessWriteBit) == AccessFlags::None);

			if (!same)
			{
				BufferBarrierDesc desc
				{
					.srcStageMask = op.stage,
					.srcAccessMask = op.access,
					.dstStageMask = target.stage,
					.dstAccessMask = target.access,
					.buffer = &buffer,
					.offset = op.offset,
					.size = op.size,
					.srcQueueFamily = QueueFamilyIgnored,
					.dstQueueFamily = QueueFamilyIgnored
				};
				BarrierCommand::buffer(dstCmd, desc);
			}
		}
	}

	void CommandEncoder::Impl::transitionBuffer(const CommandBuffer& dstCmd, Buffer& buffer, const BufferTarget& target, BufferState& state, uint64_t offset, uint64_t size)
	{
		if (size == BufferWholeSize) size = buffer.size() - offset;
		
		auto ops = merge(state, offset, size);

		transitionBufferRanges(dstCmd, buffer, ops, target, state);
		commitRange(state, target, offset, size);
	}


	void CommandEncoder::Impl::transitionPresent()
	{
		if (!presentImage) return;
		auto presentTarget = resolveImage(UsageIntent::Present);

		auto& state = stateRegistry.imageState(*presentImage);
		
		auto& s0 = state.at(0, 0);

		if (s0.layout == ImageLayout::PresentSrc) return;

		auto& cmd = queue.cmd();

		ImageBarrierDesc desc
		{
			.srcStageMask = s0.stage,
			.srcAccessMask = s0.access,

			.dstStageMask = presentTarget.stage,
			.dstAccessMask = presentTarget.access,

			.image = presentImage,

			.oldLayout = s0.layout,
			.newLayout = presentTarget.layout,

			.baseMipLevel = 0,
			.levelCount = 1,

			.baseLayer = 0,
			.layerCount = 1
		};

		BarrierCommand::image(cmd, desc);

		s0.layout = presentTarget.layout;
		s0.access = presentTarget.access;
		s0.stage = presentTarget.stage;
	}

	void CommandEncoder::Impl::transitionSubresources(const CommandBuffer& dstCmd, Image& img, const std::vector<ImageBarrierOp>& ops, const ImageTarget& target, ImageState& state)
	{
		for (const auto& op : ops)
		{
			const bool same = op.source.access == target.access && op.source.stage == target.stage
				&& op.source.layout == target.layout && ((target.access & AccessWriteBit) == AccessFlags::None);

			if (!same)
			{
				ImageBarrierDesc desc
				{
					.srcStageMask = op.source.stage,
					.srcAccessMask = op.source.access,
					.dstStageMask = target.stage,
					.dstAccessMask = target.access,
					.image = &img,

					.oldLayout = op.source.layout,
					.newLayout = target.layout,
					.srcQueueFamily = QueueFamilyIgnored,
					.dstQueueFamily = QueueFamilyIgnored,

					.aspect = aspectOf(img),

					.baseMipLevel = op.baseMip,
					.levelCount = op.mipCount,

					.baseLayer = op.baseLayer,
					.layerCount = op.layerCount
				};
				BarrierCommand::image(dstCmd, desc);
			}

			commitState(state, target, op.baseMip, op.baseMip + op.mipCount, op.baseLayer, op.baseLayer + op.layerCount);
		}
	}


	CommandEncoder::CommandEncoder(CommandQueue& queue, HazardTracker& tracker) : m_impl(new Impl{ queue, tracker })
	{
	}

	CommandEncoder::~CommandEncoder() = default;

	void CommandEncoder::setDrawable(Image& image)
	{
		ImageState state
		{
			.levelCount = 1,
			.layerCount = 1
		};

		state.subresources.resize(1);

		auto& s0_state = state.at(0, 0);
		s0_state.layout = ImageLayout::Undefined;
		s0_state.access = AccessFlags::None;
		s0_state.stage = PipelineStageFlags::ColorAttachmentOutput;

		m_impl->presentImage = &image;
		if (!m_impl->stateRegistry.emplace(*m_impl->presentImage, state))
			m_impl->stateRegistry.imageState(*m_impl->presentImage) = state;
	}

	CommandEncoder::PassBuilder CommandEncoder::encodePass(const std::function<void(CommandRecorder& recorder)>& callback)
	{
		Pass pass
		{
			.callback = callback
		};

		m_impl->passes.push_back(std::move(pass));

		return PassBuilder(this, static_cast<uint32_t>(m_impl->passes.size() - 1));
	}

	CommandEncoder::PassBuilder& CommandEncoder::PassBuilder::useImage(Image& image, UsageIntent usage, uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount)
	{
		if (mipCount == RemainingMipLevels)   mipCount = image.levelCount() - baseMip;
		if (layerCount == RemainingArrayLayers) layerCount = image.layerCount() - baseLayer;

		auto& imageUsages = m_encoder->m_impl->passes[m_index].imageUsages[&image];

		for (auto& e : imageUsages)
		{
			if (e.baseMip == baseMip && e.mipCount == mipCount &&
				e.baseLayer == baseLayer && e.layerCount == layerCount)
			{
				e.intent = e.intent | usage;
				return *this;
			}
		}

		imageUsages.push_back(SubresourceUsage{ usage, baseMip, mipCount, baseLayer, layerCount });
		
		return *this;
	}

	CommandEncoder::PassBuilder& CommandEncoder::PassBuilder::useBuffer(Buffer& buffer, UsageIntent usage, uint64_t offset, uint64_t size)
	{
		if (size == BufferWholeSize) size = buffer.size();

		auto& bufferRanges = m_encoder->m_impl->passes[m_index].bufferUsages[&buffer];

		for (auto& r : bufferRanges)
		{
			if (r.offset == offset && r.size == size)
				r.intent |= usage;
		}

		m_encoder->m_impl->passes[m_index].bufferUsages[&buffer].push_back(BufferRangeUsage{ usage, offset, size });

		return *this;
	}

	void CommandEncoder::commit()
	{
		for (auto& pass : m_impl->passes) 
		{
			const auto& cmd = m_impl->queue.cmd();

			for (auto& [buffer, usages] : pass.bufferUsages)
			{
				auto& state = m_impl->stateRegistry.bufferState(*buffer);

				for (auto& u : usages)
				{
					m_impl->transitionBuffer(cmd, *buffer, resolveBuffer(u.intent), state, u.offset, u.size);
				}
			}

			for (auto& [image, usages] : pass.imageUsages)
			{
				auto& state = m_impl->stateRegistry.imageState(*image);

				for (auto& u : usages)
				{
					m_impl->transitionImage(cmd, *image, resolveImage(u.intent), state, u.baseMip, u.mipCount, u.baseLayer, u.layerCount);
				}
			}

			CommandRecorder recorder(cmd);
			pass.callback(recorder);
			
		}
		m_impl->transitionPresent();

		m_impl->queue.submit();
	}
}
