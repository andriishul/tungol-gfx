#include "AnimationResources.hpp"

#include "RenderKernel.hpp"

#include <core/ModelRegistry.hpp>

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>
#include <vhl/graph/CommandQueue.hpp>

#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

#include <vhl/commands/CopyContext.hpp>

#include "helper/CatchGraphicsError.hpp"

namespace tungol::runtime
{
	struct AnimChannel 
	{
		uint32_t boneIndex = 0;
		uint32_t keyCount = 0;
		uint32_t keyIndex = 0;
	};

	struct Animation
	{
		float duration = 0.0f;
		uint32_t channelIndex = 0;
		uint32_t channelCount = 0;
	};

	struct Keyframe
	{
		float time;
		
		alignas(16) math::vec3 position;
		
		alignas(16) math::quat rotation;
		
		alignas(16) math::vec3 scale;
	};

	struct BoneMeta
	{
		uint32_t parentIndex = UINT32_MAX;      
		uint32_t instanceIndex = 0;    
		uint32_t localBoneIndex = 0;  
	};

	struct PlaybackState
	{
		float time;
		uint32_t clipIndex;
		uint32_t boneOffset;
		uint32_t boneCount;
	};

	struct AnimationResources::Impl 
	{
		Impl(RenderKernel& kern);
		~Impl();

		RenderKernel& kernel;

		std::unique_ptr<vhl::StorageBuffer> boneMetaSB;

		std::unique_ptr<vhl::StorageBuffer> animationsSB;
		std::unique_ptr<vhl::StorageBuffer> channelsSB;
		std::unique_ptr<vhl::StorageBuffer> keyframesSB;
		std::unique_ptr<vhl::StorageBuffer> bindPosesSB;
		std::unique_ptr<vhl::StorageBuffer> inverseBindsSB;

		std::unique_ptr<vhl::StorageBuffer> playbackSBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StagingBuffer> playbackTBs[RenderKernel::FramesInFlight];

		std::unique_ptr<vhl::StorageBuffer> boneLocalsSBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StorageBuffer> bonePaletteSBs[RenderKernel::FramesInFlight];

		vhl::DescriptorSetHandle sampleSets[RenderKernel::FramesInFlight];
		vhl::DescriptorSetHandle hierarchySets[RenderKernel::FramesInFlight];

		std::vector<PlaybackState> playbacks;
		std::vector<uint32_t> boneOffsets;

		uint32_t boneCount = 0;

		std::vector<uint32_t> resolvedClipIndices;
		std::vector<std::string> resolvedClipNames;

		void buildStatic(const core::Scene& scene, const core::ModelRegistry& models, vhl::CopyContext& cpy);
		uint8_t* buildPlayback(const core::Scene& scene, const core::ModelRegistry& models);

		core::ModelID resolveClip(core::ModelID modelID, const std::string& name, const core::ModelRegistry& models) const;
	};
	
	AnimationResources::Impl::Impl(RenderKernel& kern)
		: kernel(kern)
	{
	}

	AnimationResources::Impl::~Impl() 
	{
		try
		{
			kernel.context().activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR

		boneMetaSB.reset();
		animationsSB.reset();
		keyframesSB.reset();
		bindPosesSB.reset();
		channelsSB.reset();
		inverseBindsSB.reset();

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
		{
			playbackSBs[i].reset();
			playbackTBs[i].reset();
			boneLocalsSBs[i].reset();
			bonePaletteSBs[i].reset();

			kernel.descriptorPool().remove(sampleSets[i]);
			kernel.descriptorPool().remove(hierarchySets[i]);
		}
	}

	void AnimationResources::Impl::buildStatic(const core::Scene& scene, const core::ModelRegistry& models, vhl::CopyContext& cpy)
	{
		auto& device = kernel.context().activeDevice();
		auto& allocator = kernel.allocator();
		auto& layouts = kernel.layouts();
		auto& pool = kernel.descriptorPool();
		auto& registry = kernel.tracker();

		uint32_t instanceCount = static_cast<uint32_t>(scene.instances().size());
		
		boneCount = 0;
		for (uint32_t i = 0; i < instanceCount; ++i)
		{
			const auto& inst = scene.instances()[i];
			boneCount += static_cast<uint32_t>(models.model(inst.modelID).bones.size());
		}

		boneOffsets.assign(instanceCount, UINT32_MAX);

		uint32_t boneBase = 0;

		std::vector<BoneMeta> boneMetas;
		std::vector<math::mat4> bindPoses;
		std::vector<math::mat4> inverseBindMatrices;

		
		if (boneCount > 0)
		{
			boneMetas.reserve(boneCount);
			bindPoses.reserve(boneCount);
			inverseBindMatrices.reserve(boneCount);
		}
		else 
		{
			boneCount = 1;
			boneMetas.resize(boneCount, BoneMeta{});
			bindPoses.resize(boneCount, math::mat4::identity());
			inverseBindMatrices.resize(boneCount, math::mat4::identity());
		}

		for (uint32_t i = 0; i < instanceCount; ++i) 
		{
			const auto& inst = scene.instances()[i];
			const auto& model = models.model(inst.modelID);

			boneOffsets[i] = boneBase;

			if (model.skinnedVertices.empty()) continue;

			for (uint32_t j = 0; j < static_cast<uint32_t>(model.bones.size()); ++j)
			{
				const auto& bone = model.bones[j];

				BoneMeta meta{};
				meta.parentIndex = (bone.parentIndex < 0) ? UINT32_MAX : boneBase + static_cast<uint32_t>(bone.parentIndex);
				meta.instanceIndex = i;
				meta.localBoneIndex = j;

				boneMetas.push_back(meta);
				bindPoses.push_back(bone.bindLocalTransform);
				inverseBindMatrices.push_back(bone.inverseBindMatrix);
			}

			boneBase += static_cast<uint32_t>(model.bones.size());
		}

		uint32_t frameBase = 0;
		uint32_t channelBase = 0;

		resolvedClipIndices.assign(instanceCount, UINT32_MAX);
		resolvedClipNames.assign(instanceCount, "");

		std::vector<AnimChannel> channels;
		std::vector<Animation> animations;
		std::vector<Keyframe> keyframes;


		for (const auto& entry : models.all()) 
		{
			const auto& model = entry.model;

			for (const auto& ch : model.channels) 
			{
				AnimChannel c{};

				c.boneIndex = ch.boneIndex;
				c.keyCount = ch.keyCount;
				c.keyIndex = ch.keyIndex;

				c.keyIndex += frameBase;                
				channels.push_back(c);
			}

			for (uint32_t i = 0; i < static_cast<uint32_t>(model.animations.size()); ++i)
			{
				const auto& anim = model.animations[i];

				Animation a{};
				a.duration = anim.duration;
				a.channelCount = anim.channelCount;
				a.channelIndex = anim.channelIndex;

				a.channelIndex += channelBase;
				animations.push_back(a);
			}

			for (const auto& key : model.frames)
			{
				Keyframe kf{};
				kf.time = key.time;
				kf.position = key.position;
				kf.scale = key.scale;
				kf.rotation = key.rotation;

				keyframes.push_back(kf);
			}
			frameBase += static_cast<uint32_t>(model.frames.size());
			channelBase += static_cast<uint32_t>(model.channels.size());
		}

		if (animations.empty()) animations.push_back(Animation{});
		if (channels.empty()) channels.push_back(AnimChannel{});
		if (keyframes.empty()) keyframes.push_back(Keyframe{});

		

		boneMetaSB = std::make_unique<vhl::StorageBuffer>(device, allocator, boneMetas.size() * sizeof(BoneMeta));
		cpy.upload(*boneMetaSB, boneMetas.data(), boneMetas.size() * sizeof(BoneMeta));

		bindPosesSB = std::make_unique<vhl::StorageBuffer>(device, allocator, bindPoses.size() * sizeof(math::mat4));
		cpy.upload(*bindPosesSB, bindPoses.data(), bindPoses.size() * sizeof(math::mat4));

		inverseBindsSB = std::make_unique<vhl::StorageBuffer>(device, allocator, inverseBindMatrices.size() * sizeof(math::mat4));
		cpy.upload(*inverseBindsSB, inverseBindMatrices.data(), inverseBindMatrices.size() * sizeof(math::mat4));

		animationsSB = std::make_unique<vhl::StorageBuffer>(device, allocator, animations.size() * sizeof(Animation));
		cpy.upload(*animationsSB, animations.data(), animations.size() * sizeof(Animation));

		channelsSB = std::make_unique<vhl::StorageBuffer>(device, allocator, channels.size() * sizeof(AnimChannel));
		cpy.upload(*channelsSB, channels.data(), channels.size() * sizeof(AnimChannel));

		keyframesSB = std::make_unique<vhl::StorageBuffer>(device, allocator, keyframes.size() * sizeof(Keyframe));
		cpy.upload(*keyframesSB, keyframes.data(), keyframes.size() * sizeof(Keyframe));

		
		const uint64_t bonesSize = static_cast<uint64_t>(boneCount) * sizeof(math::mat4);
		const uint64_t playbacksSize = std::max<uint64_t>(static_cast<uint64_t>(instanceCount) * sizeof(PlaybackState), sizeof(PlaybackState));

		for (uint32_t f = 0; f < RenderKernel::FramesInFlight; ++f)
		{
			playbackSBs[f] = std::make_unique<vhl::StorageBuffer>(device, allocator, playbacksSize);
			playbackTBs[f] = std::make_unique<vhl::StagingBuffer>(device, allocator, playbacksSize);
			boneLocalsSBs[f] = std::make_unique<vhl::StorageBuffer>(device, allocator, bonesSize);
			bonePaletteSBs[f] = std::make_unique<vhl::StorageBuffer>(device, allocator, bonesSize);

			registry.emplace(*playbackTBs[f]);
			registry.emplace(*playbackSBs[f]);
			registry.emplace(*boneLocalsSBs[f]);
			registry.emplace(*bonePaletteSBs[f]);

			sampleSets[f] = pool.emplace(layouts[3]);
			vhl::DescriptorWriter(sampleSets[f])
				.storageBuffer(*boneMetaSB, 0, 0, boneMetaSB->size())
				.storageBuffer(*playbackSBs[f], 1, 0, playbacksSize)
				.storageBuffer(*animationsSB, 2, 0, animationsSB->size())
				.storageBuffer(*channelsSB, 3, 0, channelsSB->size())
				.storageBuffer(*keyframesSB, 4, 0, keyframesSB->size())
				.storageBuffer(*bindPosesSB, 5, 0, bindPosesSB->size())
				.storageBuffer(*boneLocalsSBs[f], 6, 0, bonesSize)
				.apply(device);

			hierarchySets[f] = pool.emplace(layouts[4]);
			vhl::DescriptorWriter(hierarchySets[f])
				.storageBuffer(*boneMetaSB, 0, 0, boneMetaSB->size())
				.storageBuffer(*boneLocalsSBs[f], 6, 0, bonesSize)
				.storageBuffer(*bonePaletteSBs[f], 7, 0, bonesSize)
				.storageBuffer(*inverseBindsSB, 8, 0, inverseBindsSB->size())
				.apply(device);
		}
	}

	uint8_t* AnimationResources::Impl::buildPlayback(const core::Scene& scene, const core::ModelRegistry& models)
	{
		playbacks.clear();

		for (uint32_t i = 0; i < static_cast<uint32_t>(scene.instances().size()); ++i) 
		{
			const auto& inst = scene.instances()[i];
			const auto& model = models.model(inst.modelID);
			const auto& boneOffset = boneOffsets[i];

			if (resolvedClipNames[i] != inst.clipName)
			{
				resolvedClipIndices[i] = resolveClip(inst.modelID, inst.clipName, models);
				resolvedClipNames[i] = inst.clipName;
			}
			
			PlaybackState pb{};
			pb.time = inst.time;
			pb.clipIndex = resolvedClipIndices[i];
			pb.boneOffset = boneOffset;
			pb.boneCount = static_cast<uint32_t>(model.bones.size());
			playbacks.push_back(pb);
		}

		if (playbacks.empty()) return nullptr;

		return reinterpret_cast<uint8_t*>(playbacks.data());
	}

	AnimationResources::~AnimationResources() = default;

	void AnimationResources::buildStatic(const core::Scene& scene, const core::ModelRegistry& models, vhl::CopyContext& cpy)
	{
		if (m_impl) m_impl->buildStatic(scene, models, cpy);
	}

	uint8_t* AnimationResources::buildPlayback(const core::Scene& scene, const core::ModelRegistry& models)
	{
		return m_impl ? m_impl->buildPlayback(scene, models) : nullptr;
	}

	uint64_t AnimationResources::playbackSize() const
	{
		return m_impl ? static_cast<uint64_t>(m_impl->playbacks.size()) * sizeof(PlaybackState) : 0ULL;
	}


	AnimationResources::AnimationResources(RenderKernel& kern) : m_impl(new Impl(kern))
	{
	
	}

	core::ModelID AnimationResources::Impl::resolveClip(core::ModelID modelId, const std::string& name, const core::ModelRegistry& models) const
	{
		core::ModelID base = 0;
		for (core::ModelID i = 0; i < modelId; ++i)
			base += static_cast<uint32_t>(models.model(i).animations.size());

		const auto& anims = models.model(modelId).animations;
		for (core::ModelID i = 0; i < static_cast<core::ModelID>(anims.size()); ++i)
			if (name == anims[i].name)
				return base + i;

		return UINT32_MAX;
	}

	vhl::Buffer& AnimationResources::boneMetaSB() const 
	{
		if (!m_impl->boneMetaSB) throw std::runtime_error("[AnimationResources::boneMetaSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->boneMetaSB; 
	}
	vhl::Buffer& AnimationResources::animationsSB() const 
	{
		if (!m_impl->animationsSB) throw std::runtime_error("[AnimationResources::animationsSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->animationsSB; 
	}

	vhl::Buffer& AnimationResources::channelsSB() const 
	{
		if (!m_impl->channelsSB) throw std::runtime_error("[AnimationResources::channelsSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->channelsSB; 
	}

	vhl::Buffer& AnimationResources::keyframesSB() const 
	{
		if (!m_impl->keyframesSB) throw std::runtime_error("[AnimationResources::keyframesSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->keyframesSB; 
	}
	vhl::Buffer& AnimationResources::bindPosesSB() const 
	{
		if (!m_impl->bindPosesSB) throw std::runtime_error("[AnimationResources::bindPosesSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->bindPosesSB; 
	}
	vhl::Buffer& AnimationResources::inverseBindsSB() const 
	{
		if (!m_impl->inverseBindsSB) throw std::runtime_error("[AnimationResources::inverseBindsSB] Not initialized. Did you commit the scene before using it?");
		return *m_impl->inverseBindsSB; 
	}

	vhl::Buffer& AnimationResources::playbackSBs(uint32_t slot) const 
	{ 
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationResources::playbackSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->playbackSBs[slot]) throw std::runtime_error("[AnimationResources::playbackSBs] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?");
		return *m_impl->playbackSBs[slot]; 
	}
	vhl::Buffer& AnimationResources::playbackTBs(uint32_t slot) const 
	{
		if (slot >= RenderKernel::FramesInFlight) abort(); // throw std::out_of_range("[AnimationResources::playbackTBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->playbackTBs[slot]) throw std::runtime_error("[AnimationResources::playbackTBs] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?");
		return *m_impl->playbackTBs[slot]; 
	}
	vhl::Buffer& AnimationResources::boneLocalsSBs(uint32_t slot) const 
	{ 
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationResources::boneLocalsSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->boneLocalsSBs[slot]) throw std::runtime_error("[AnimationResources::boneLocalsSBs] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?");
		return *m_impl->boneLocalsSBs[slot];
	}
	vhl::Buffer& AnimationResources::bonePaletteSBs(uint32_t slot) const 
	{ 
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationResources::bonePaletteSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->bonePaletteSBs[slot]) throw std::runtime_error("[AnimationResources::bonePaletteSBs] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?");
		return *m_impl->bonePaletteSBs[slot]; 
	}

	const uint32_t AnimationResources::boneCount() const
	{
		return m_impl->boneCount;
	}

	const uint32_t AnimationResources::boneOffset(uint32_t instanceIndex)
	{
		if (instanceIndex >= m_impl->boneOffsets.size()) 
			throw std::out_of_range("[AnimationResources::boneOffset] instanceIndex out of range ("+ std::to_string(instanceIndex) + " >= "+ std::to_string(m_impl->boneOffsets.size()) + ")");
		return m_impl->boneOffsets[instanceIndex];
	}
	
	const vhl::DescriptorSetHandle& AnimationResources::sampleSets(uint32_t slot) const 
	{ 
		if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[AnimationResources::sampleSets] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->sampleSets[slot].value) throw std::runtime_error("[AnimationResources::sampleSets] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?"); 
		return m_impl->sampleSets[slot]; 
	}
	const vhl::DescriptorSetHandle& AnimationResources::hierarchySets(uint32_t slot) const 
	{ 
		if (slot >= RenderKernel::FramesInFlight) throw std::runtime_error("[AnimationResources::hierarchySets] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
		if (!m_impl->hierarchySets[slot].value) throw std::runtime_error("[AnimationResources::hierarchySets] slot " + std::to_string(slot) + " not initialized. Did you commit the scene before using it?");
		return m_impl->hierarchySets[slot]; 
	}
}
