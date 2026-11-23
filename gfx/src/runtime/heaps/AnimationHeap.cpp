#include "AnimationHeap.hpp"

#include <runtime/RenderKernel.hpp>
#include <runtime/internal/KernelInternal.hpp>

#include <runtime/data/AnimationData.hpp>

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/core/Allocator.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

#include <stdexcept>

namespace tungol::runtime
{
	struct AnimationHeap::Impl
	{
		Impl(RenderKernel& kern, const AnimationHeapSizes& sizes);

		~Impl();

		vhl::DescriptorPool& descriptorPool;
		AnimationHeapSizes sizes;
		
		std::unique_ptr<Pool> boneMetasPool, animationsPool, channelsPool, keyframesPool, bindPosesPool, inverseBindsPool;

		std::unique_ptr<vhl::StorageBuffer> playbackSBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StagingBuffer> playbackTBs[RenderKernel::FramesInFlight];

		std::unique_ptr<vhl::StorageBuffer> boneLocalsSBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StorageBuffer> bonePaletteSBs[RenderKernel::FramesInFlight];

		vhl::DescriptorSetHandle sampleSets[RenderKernel::FramesInFlight];
		vhl::DescriptorSetHandle hierarchySets[RenderKernel::FramesInFlight];

		AnimationAllocation allocate(const AnimationSizes& sizes);
		void free(const AnimationAllocation& a);
	};

	AnimationHeap::Impl::Impl(RenderKernel& kern, const AnimationHeapSizes& s) :
		descriptorPool(KernelInternal::descriptorPool(kern)), sizes(s)
	{
		auto& device = KernelInternal::context(kern).activeDevice();

		auto& tracker = KernelInternal::tracker(kern);
		auto& allocator = KernelInternal::allocator(kern);
		auto& layouts = KernelInternal::layouts(kern);

		std::unique_ptr<vhl::Buffer> boneMetasSB(new vhl::StorageBuffer(device, allocator, s.boneMetasSize));
		tracker.emplace(*boneMetasSB);
		boneMetasPool = std::make_unique<Pool>(std::move(boneMetasSB), s.boneMetasSize, sizeof(ShaderBoneMeta));

		std::unique_ptr<vhl::Buffer> animationsSB(new vhl::StorageBuffer(device, allocator, s.animationsSize));
		tracker.emplace(*animationsSB);
		animationsPool = std::make_unique<Pool>(std::move(animationsSB), s.animationsSize, sizeof(ShaderAnimation));

		std::unique_ptr<vhl::Buffer> channelsSB(new vhl::StorageBuffer(device, allocator, s.channelsSize));
		tracker.emplace(*channelsSB);
		channelsPool = std::make_unique<Pool>(std::move(channelsSB), s.channelsSize, sizeof(ShaderChannel));

		std::unique_ptr<vhl::Buffer> keyframesSB(new vhl::StorageBuffer(device, allocator, s.keyframesSize));
		tracker.emplace(*keyframesSB);
		keyframesPool = std::make_unique<Pool>(std::move(keyframesSB), s.keyframesSize, sizeof(ShaderKeyframe));

		std::unique_ptr<vhl::Buffer> bindPosesSB(new vhl::StorageBuffer(device, allocator, s.bindPosesSize));
		tracker.emplace(*bindPosesSB);
		bindPosesPool = std::make_unique<Pool>(std::move(bindPosesSB), s.bindPosesSize, sizeof(math::mat4));

		std::unique_ptr<vhl::Buffer> inverseBindsSB(new vhl::StorageBuffer(device, allocator, s.inverseBindsSize));
		tracker.emplace(*inverseBindsSB);
		inverseBindsPool = std::make_unique<Pool>(std::move(inverseBindsSB), s.inverseBindsSize, sizeof(math::mat4));

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
		{
			playbackTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, s.playbackSize);

			playbackSBs[i] = std::make_unique<vhl::StorageBuffer>(device, allocator, s.playbackSize);
			boneLocalsSBs[i] = std::make_unique<vhl::StorageBuffer>(device, allocator, s.boneLocalsSize);
			bonePaletteSBs[i] = std::make_unique<vhl::StorageBuffer>(device, allocator, s.bonePaletteSize);

			tracker.emplace(*playbackTBs[i]);
			tracker.emplace(*playbackSBs[i]);

			tracker.emplace(*boneLocalsSBs[i]);
			tracker.emplace(*bonePaletteSBs[i]);

			sampleSets[i] = descriptorPool.emplace(layouts[3]);
			vhl::DescriptorWriter(sampleSets[i])
				.storageBuffer(boneMetasPool->buffer(), 0, 0, s.boneMetasSize)
				.storageBuffer(*playbackSBs[i], 1, 0, s.playbackSize)
				.storageBuffer(animationsPool->buffer(), 2, 0, s.animationsSize)
				.storageBuffer(channelsPool->buffer(), 3, 0, s.channelsSize)
				.storageBuffer(keyframesPool->buffer(), 4, 0, s.keyframesSize)
				.storageBuffer(bindPosesPool->buffer(), 5, 0, s.bindPosesSize)
				.storageBuffer(*boneLocalsSBs[i], 6, 0, s.boneLocalsSize)
				.apply(device);

			hierarchySets[i] = descriptorPool.emplace(layouts[4]);
			vhl::DescriptorWriter(hierarchySets[i])
				.storageBuffer(boneMetasPool->buffer(), 0, 0, s.boneMetasSize)
				.storageBuffer(*playbackSBs[i], 1, 0, s.playbackSize)
				.storageBuffer(*boneLocalsSBs[i], 6, 0, s.boneLocalsSize)
				.storageBuffer(*bonePaletteSBs[i], 7, 0, s.bonePaletteSize)
				.storageBuffer(inverseBindsPool->buffer(), 8, 0, s.inverseBindsSize)
				.apply(device);
		}
	}

	AnimationHeap::Impl::~Impl()
	{
		boneMetasPool.reset();
		animationsPool.reset();
		channelsPool.reset();
		keyframesPool.reset();
		bindPosesPool.reset();
		inverseBindsPool.reset();

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
		{
			playbackSBs[i].reset();
			boneLocalsSBs[i].reset();
			bonePaletteSBs[i].reset();

			descriptorPool.remove(sampleSets[i]);
			descriptorPool.remove(hierarchySets[i]);
		}
	}


	AnimationHeap::~AnimationHeap() = default;

	AnimationAllocation AnimationHeap::Impl::allocate(const AnimationSizes& sizes)
	{
		AnimationAllocation allocation{};

		if (sizes.boneMetasSize > 0 && boneMetasPool->canAllocate(sizes.boneMetasSize))
			allocation.boneMetas = *boneMetasPool->allocate(sizes.boneMetasSize);

		if (sizes.animationsSize > 0 && animationsPool->canAllocate(sizes.animationsSize))
			allocation.animations = *animationsPool->allocate(sizes.animationsSize);

		if (sizes.channelsSize > 0 && channelsPool->canAllocate(sizes.channelsSize))
			allocation.channels = *channelsPool->allocate(sizes.channelsSize);

		if (sizes.keyframesSize > 0 && keyframesPool->canAllocate(sizes.keyframesSize))
			allocation.keyframes = *keyframesPool->allocate(sizes.keyframesSize);

		if (sizes.bindPosesSize > 0 && bindPosesPool->canAllocate(sizes.bindPosesSize))
			allocation.bindPoses = *bindPosesPool->allocate(sizes.bindPosesSize);

		if (sizes.inverseBindsSize > 0 && inverseBindsPool->canAllocate(sizes.inverseBindsSize))
			allocation.inverseBinds = *inverseBindsPool->allocate(sizes.inverseBindsSize);

		return allocation;
	}

	void AnimationHeap::Impl::free(const AnimationAllocation& a)
	{
		if (a.boneMetas.has_value() && a.boneMetas->size > 0) boneMetasPool->free(*a.boneMetas);
		if (a.animations.has_value() && a.animations->size > 0) animationsPool->free(*a.animations);
		if (a.bindPoses.has_value() && a.bindPoses->size > 0) bindPosesPool->free(*a.bindPoses);

		if (a.channels.has_value() && a.channels->size > 0) channelsPool->free(*a.channels);
		if (a.inverseBinds.has_value() && a.inverseBinds->size > 0) inverseBindsPool->free(*a.inverseBinds);
		if (a.keyframes.has_value() && a.keyframes->size > 0) keyframesPool->free(*a.keyframes);
	}


	AnimationHeap::AnimationHeap(RenderKernel& kernel, const AnimationHeapSizes& heapSizes)
		: m_impl(new Impl(kernel, heapSizes))
	{
	}

	AnimationAllocation AnimationHeap::allocate(const AnimationSizes& sizes)
	{
		return m_impl->allocate(sizes);
	}


	void AnimationHeap::free(const AnimationAllocation& a)
	{
		m_impl->free(a);
	}

	const Pool& AnimationHeap::boneMetasPool() const
	{
		return *m_impl->boneMetasPool;
	}

	const Pool& AnimationHeap::animationsPool() const
	{
		return *m_impl->animationsPool;
	}

	const Pool& AnimationHeap::channelsPool() const
	{
		return *m_impl->channelsPool;
	}

	const Pool& AnimationHeap::keyframesPool() const
	{
		return *m_impl->keyframesPool;
	}

	const Pool& AnimationHeap::bindPosesPool() const
	{
		return *m_impl->bindPosesPool;
	}

	const Pool& AnimationHeap::inverseBindsPool() const
	{
		return *m_impl->inverseBindsPool;
	}

	
	vhl::Buffer& AnimationHeap::playbackSBs(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationHeap::playbackSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->playbackSBs[slot]) throw std::runtime_error("[AnimationHeap::playbackSBs] slot " + std::to_string(slot) + " not initialized.");

		return *m_impl->playbackSBs[slot];
	}

	vhl::Buffer& AnimationHeap::playbackTBs(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationHeap::playbackTBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->playbackTBs[slot]) throw std::runtime_error("[AnimationHeap::playbackTBs] slot " + std::to_string(slot) + " not initialized.");
		return *m_impl->playbackTBs[slot];
	}

	vhl::Buffer& AnimationHeap::boneLocalsSBs(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationHeap::boneLocalsSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->boneLocalsSBs[slot]) throw std::runtime_error("[AnimationHeap::boneLocalsSBs] slot " + std::to_string(slot) + " not initialized.");

		return *m_impl->boneLocalsSBs[slot];
	}

	vhl::Buffer& AnimationHeap::bonePaletteSBs(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationHeap::bonePaletteSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->bonePaletteSBs[slot]) throw std::runtime_error("[AnimationHeap::bonePaletteSBs] slot " + std::to_string(slot) + " not initialized.");

		return *m_impl->bonePaletteSBs[slot];
	}

	const vhl::DescriptorSetHandle& AnimationHeap::sampleSets(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationHeap::sampleSets] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->sampleSets[slot].value) throw std::runtime_error("[AnimationHeap::sampleSets] slot " + std::to_string(slot) + " not initialized.");
		
		return m_impl->sampleSets[slot];
	}
	const vhl::DescriptorSetHandle& AnimationHeap::hierarchySets(uint32_t slot) const
	{
		if (slot >= RenderKernel::FramesInFlight) throw std::runtime_error("[AnimationHeap::hierarchySets] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->hierarchySets[slot].value) throw std::runtime_error("[AnimationHeap::hierarchySets] slot " + std::to_string(slot) + " not initialized.");
		
		return m_impl->hierarchySets[slot];
	}
	AnimationHeapSizes AnimationHeap::sizes() const
	{
		return m_impl->sizes;
	}
}