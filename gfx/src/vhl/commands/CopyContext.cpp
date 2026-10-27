#include "CopyContext.hpp"

#include "BarrierCommand.hpp"
#include "CommandRecorder.hpp"
#include <vhl/sync/Fence.hpp>

#include <vhl/core/buffers/StagingBuffer.hpp>

#include <unordered_set>
#include <stdexcept>

namespace tungol::vhl 
{
	constexpr uint64_t TRANSFER_STAGING_SIZE = 64 * 1024 * 1024; // 64 MB

	template<typename R>
	struct Pending
	{
		R* resource = nullptr;

		uint64_t srcOffset = 0;
		uint64_t dstOffset = 0;
		uint64_t size = 0;

		uint32_t mipLevel = 0;
		uint32_t arrayLayer = 0;
	};

	struct CopyContext::Impl
	{
	private:
		const Device& device;

		CommandQueue& queue;

		HazardTracker& tracker;

		std::vector<Pending<Buffer>> pendingBuffers;
		std::vector<Pending<Image>> pendingImages;

		std::unique_ptr<StagingBuffer> stagingBuffer;
		std::vector<uint8_t> bytes;
	public:
		Impl(const Device& device, CommandQueue& cmdQueue, HazardTracker& tracker);

		void allocStagingBuffer(SegregatedAllocator& allocator);

		void transitionToTransfer(const CommandBuffer& cmd, Buffer& buf);
		void transitionToTransfer(const CommandBuffer& cmd, Image& img);

		void upload(Buffer& buf, const void* pData, uint64_t size, uint64_t offset);
		void upload(Image& img, const void* pData, uint64_t size, uint32_t mipLevel, uint32_t arrayLayer);
		void flush();
	};


	static uint64_t stageCopy(uint8_t* base, std::vector<uint8_t>& bytes, uint64_t src, uint64_t size, uint64_t& baseOffset)
	{
		if (baseOffset + size > TRANSFER_STAGING_SIZE) 
			throw std::runtime_error("[TransferBatch] staging overflow.");
		
		memcpy(base + baseOffset, bytes.data() + src, size);
		uint64_t where = baseOffset;
		baseOffset += size;
		return where;
	};


#pragma region CopyContext::Impl

	CopyContext::Impl::Impl(const Device& dev, CommandQueue& cmdQueue, HazardTracker& tracker)
		: device(dev), queue(cmdQueue), tracker(tracker)
	{
	}

	void CopyContext::Impl::allocStagingBuffer(SegregatedAllocator& allocator)
	{
		stagingBuffer = std::make_unique<StagingBuffer>(device, allocator, TRANSFER_STAGING_SIZE);
	}
	
	void CopyContext::Impl::transitionToTransfer(const CommandBuffer& cmd, Buffer& buf)
	{
		BufferBarrierDesc desc
		{
			.srcStageMask = PipelineStageFlags::None,
			.srcAccessMask = AccessFlags::None,

			.dstStageMask = PipelineStageFlags::Transfer,
			.dstAccessMask = AccessFlags::TransferWrite,

			.buffer = &buf
		};

		BarrierCommand::buffer(cmd, desc);

		tracker.emplace(buf, BufferState{ desc.dstAccessMask, desc.dstStageMask });
	}

	void CopyContext::Impl::transitionToTransfer(const CommandBuffer& cmd, Image& img)
	{
		ImageBarrierDesc desc
		{
			.srcStageMask = PipelineStageFlags::None,
			.srcAccessMask = AccessFlags::None,

			.dstStageMask = PipelineStageFlags::Transfer,
			.dstAccessMask = AccessFlags::TransferWrite,

			.image = &img,

			.oldLayout = ImageLayout::Undefined,
			.newLayout = ImageLayout::TransferDst
		};

		BarrierCommand::image(cmd, desc);

		ImageState imageState
		{
			.levelCount = img.levelCount(),
			.layerCount = img.layerCount()
		};

		imageState.subresources.resize(imageState.layerCount * imageState.levelCount);

		for (uint32_t mip = 0; mip < img.levelCount(); ++mip) 
		{
			for (uint32_t layer = 0; layer < img.layerCount(); ++layer)
				imageState.at(mip, layer) = SubresourceState{ desc.newLayout, desc.dstAccessMask, desc.dstStageMask };
		}
		
		tracker.emplace(img, imageState);
	}

	void CopyContext::Impl::upload(Buffer& buf, const void* pData, uint64_t size, uint64_t offset)
	{
		if (offset + size > buf.size())
			throw std::runtime_error("[TransferBatch::upload] write out of bounds");

		uint64_t src = bytes.size();
		const uint8_t* p = static_cast<const uint8_t*>(pData);
		bytes.insert(bytes.end(), p, p + size);

		pendingBuffers.push_back({ &buf, src, offset, size });
	}

	void CopyContext::Impl::upload(Image& img, const void* pData, uint64_t size, uint32_t mipLevel, uint32_t arrayLayer)
	{
		uint64_t src = bytes.size();
		const uint8_t* p = static_cast<const uint8_t*>(pData);
		bytes.insert(bytes.end(), p, p + size);

		pendingImages.push_back({ &img, src, 0, size, mipLevel, arrayLayer });
	}

	void CopyContext::Impl::flush()
	{
		if (pendingBuffers.empty() && pendingImages.empty())
			return;

		auto& staging = *stagingBuffer;

		tracker.emplace(staging, BufferState{});

		for (const auto& pB : pendingBuffers)
			tracker.emplace(*pB.resource, BufferState{});

		for (const auto& pI : pendingImages)
			tracker.emplace(*pI.resource);


		uint64_t baseOffset = 0;
		uint8_t* base = static_cast<uint8_t*>(staging.pMappedData());

		queue.resetCmd();
		auto& cmd = queue.cmd();


		cmd.begin();

		CommandRecorder recoder(cmd);
		
		for (const auto& pB : pendingBuffers)
		{
			uint64_t stagingOffset = stageCopy(base, bytes, pB.srcOffset, pB.size, baseOffset);
			recoder.copyTo(staging, *pB.resource, stagingOffset, pB.dstOffset, pB.size);
		}

		std::unordered_set<Image*> transitionedImages;

		for (const auto& pI : pendingImages)
		{
			if (transitionedImages.insert(pI.resource).second)
			{
				transitionToTransfer(cmd, *pI.resource);
			}

			uint64_t stagingOffset = stageCopy(base, bytes, pI.srcOffset, pI.size, baseOffset);

			const uint32_t w = std::max(1u, pI.resource->width() >> pI.mipLevel);
			const uint32_t h = std::max(1u, pI.resource->height() >> pI.mipLevel);
			const uint32_t d = std::max(1u, pI.resource->depth() >> pI.mipLevel);

			recoder.copyTo(staging, *pI.resource, stagingOffset, w, h, d, 0, 0, 0, pI.mipLevel, pI.arrayLayer, 1);
		}

		cmd.end();

		Fence fence(device);
		fence.reset();
		SubmitInfo submit{ 1, &cmd, };
		submit.fence = &fence;

		auto& nativeQueue = queue.native();
		nativeQueue.submit(submit);
		
		fence.wait();

		pendingBuffers.clear();
		pendingImages.clear();
		bytes.clear();
	}

#pragma endregion


	CopyContext::CopyContext(const Device& device, SegregatedAllocator& allocator, CommandQueue& cmdQueue, HazardTracker& tracker)
		: m_impl(new Impl(device, cmdQueue, tracker))
	{
		m_impl->allocStagingBuffer(allocator);
	}

	CopyContext::~CopyContext() = default;

	CopyContext& CopyContext::upload(Buffer& buf, const void* pData, uint64_t size, uint64_t offset)
	{
		m_impl->upload(buf, pData, size, offset);
		return *this;
	}

	CopyContext& CopyContext::upload(Image& img, const void* pData, uint64_t size, uint32_t mipLevel, uint32_t arrayLayer)
	{
		m_impl->upload(img, pData, size, mipLevel,arrayLayer);
		return *this;
	}

	
	void CopyContext::flush()
	{
		m_impl->flush();
	}


	
}
