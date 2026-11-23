#include "internal/ResidencyInternal.hpp"

#include "RenderConfig.hpp"
#include "RenderKernel.hpp"
#include "PoolBudget.hpp"
#include "internal/KernelInternal.hpp"

// Heaps
#include <runtime/heaps/AnimationHeap.hpp>
#include <runtime/heaps/DrawHeap.hpp>
#include <runtime/heaps/MaterialHeap.hpp>
#include <runtime/heaps/GeometryHeap.hpp>

#include "Texture.hpp"

// Data
#include "data/FrameDrawData.hpp"
#include "data/MaterialData.hpp"
#include "data/AnimationData.hpp"
#include <vhl/commands/DrawIndirectCommand.hpp>


// Graphics
#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>
#include <vhl/core/Descriptors.hpp>
#include <vhl/graph/CommandQueue.hpp>

#include <vhl/core/Allocator.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/Sampler.hpp>
#include <vhl/core/images/ImageView.hpp>

#include <vhl/core/buffers/IndexBuffer.hpp>
#include <vhl/core/buffers/VertexBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

#include <unordered_set>

namespace tungol::runtime 
{
	static constexpr uint64_t TEX_STAGING_SIZE = 64 * 1024 * 1024;
	static constexpr uint64_t GEOM_STAGING_SIZE = 16 * 1024 * 1024;
	static constexpr uint64_t ANIM_STAGING_SIZE = 8 * 1024 * 1024;

	namespace 
	{
		struct RetireEntry
		{
			core::ModelID id;
			
			MaterialAllocation materials;
			GeometryAllocation geometry;
			AnimationAllocation animation;
			
			std::vector<uint32_t> textureSlots;

			uint32_t boneCount;
		};


		static void insertToBatch(UploadBatch& batch, vhl::Buffer& staging, uint8_t* base, uint64_t& cursor, 
			const vhl::Buffer& dst, uint64_t dstOffset, const void* src, uint64_t size)
		{
			if (size == 0) return;
			memcpy(base + cursor, src, size);
				
			batch.buffers.push_back({ &staging, cursor, const_cast<vhl::Buffer*>(&dst), dstOffset, size });

			cursor += size;
		}


		static void insertToBatch(UploadBatch& batch, vhl::Buffer& staging, uint8_t* base, uint64_t& cursor, vhl::Image& dst, 
			uint32_t mip, uint32_t layer, uint32_t width, uint32_t height, uint32_t depth, const void* src, uint64_t size)
		{
			if (size == 0) return;

			memcpy(base + cursor, src, size);

			batch.images.push_back(
			{
				&staging, cursor,
				&dst, mip, layer, 
				width, height, depth, 
				size
			});

			cursor += size;
		}

	}

	struct ResidencyManager::Impl 
	{
		Impl(RenderKernel& k, uint32_t maxTexturesNum);
		~Impl();

		uint64_t version = 0;

		const vhl::CommandQueue& directQueue;

		const vhl::Device& device;
		vhl::SegregatedAllocator& allocator;

		std::unordered_map<core::ModelID, std::unordered_map<std::string, uint32_t>> clipNames;
		std::unordered_map<core::ModelID, ResidentModel> models;
		std::unordered_map<core::ModelID, const core::ModelRecord*> pending;

		std::array<std::vector<RetireEntry>, RenderKernel::FramesInFlight> retireLists;

		std::unique_ptr<vhl::StagingBuffer> geometryTBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StagingBuffer> animationsTBs[RenderKernel::FramesInFlight];
		std::unique_ptr<vhl::StagingBuffer> texturesTBs[RenderKernel::FramesInFlight];

		uint64_t geometryOffsets[RenderKernel::FramesInFlight] = {};
		uint64_t animationsOffsets[RenderKernel::FramesInFlight] = {};
		uint64_t texturesOffsets[RenderKernel::FramesInFlight] = {};

		std::unique_ptr<AnimationHeap> animationHeap;
		std::unique_ptr<GeometryHeap> geometryHeap;
		std::unique_ptr<DrawHeap> drawHeap;

		std::unique_ptr<MaterialHeap> materialHeap;
		std::unique_ptr<TextureList> textures;
		std::unordered_map<std::string, uint32_t> cachedSlots;

		void retain(core::ModelID id, const core::ModelRecord& record);

		void releaseRetired(uint32_t frameSlot);

		void release(core::ModelID id);
		bool isResident(core::ModelID id) const;

		const ResidentModel* find(core::ModelID id) const;

		UploadBatch collectUploads(uint32_t frameSlot);

		void buildClipNames(core::ModelID id, const asset::Model& model);
		std::optional<uint32_t> clipIndex(core::ModelID id, std::string_view name) const;
		void resetStaging(uint32_t slot);

		uint32_t insertTexture(const std::string& path, const asset::BcTexture& texture, UploadBatch& batch, vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset);
		uint32_t resolveTexture(const core::ModelRecord& record, const char* path, UploadBatch& batch, vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset);
		uint32_t resolveRetain(const core::ModelRecord& record, const char* path, UploadBatch& batch, vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset, std::unordered_set<uint32_t>& retainedSlots, std::vector<uint32_t>& modelSlots);
	};

	ResidencyManager::Impl::Impl(RenderKernel& k, uint32_t maxTexturesNum) : directQueue(KernelInternal::directQueue(k)), 
		device(KernelInternal::context(k).activeDevice()), allocator(KernelInternal::allocator(k))
	{
		auto& tracker = KernelInternal::tracker(k);

		auto budgets = createDefaultPoolBudgets();

		auto budget = pickBudget(budgets, allocator.topology().heapSize(vhl::MemoryType::GpuOnly));

		GeometryHeapSizes gSizes{};
		gSizes.staticVBSize = budget.staticVBSize;
		gSizes.skinnedVBSize = budget.skinnedVBSize;
		gSizes.indexBufSize = budget.indexBufSize;

		AnimationHeapSizes aSizes{};
		aSizes.keyframesSize = budget.keyframesSize;
		aSizes.channelsSize = budget.keyframesSize / 4;
		aSizes.bindPosesSize = budget.keyframesSize / 4;
		aSizes.inverseBindsSize = budget.keyframesSize / 4;
		aSizes.boneMetasSize = budget.keyframesSize / 8;
		aSizes.animationsSize = budget.keyframesSize / 16;

		aSizes.playbackSize = main::k_MaxPlaybackCount * sizeof(ShaderPlaybackState);
		aSizes.boneLocalsSize = main::k_MaxSkinnedInstances * main::k_MaxBonesPerInstance * sizeof(math::mat4);
		aSizes.bonePaletteSize = main::k_MaxSkinnedInstances * main::k_MaxBonesPerInstance * sizeof(math::mat4);

		DrawHeapSizes dSizes{};
		dSizes.indirectSize = main::k_MaxInstances * main::k_MaxMeshesPerInstance * sizeof(vhl::DrawIndexedIndirectCommand);
		dSizes.instancesSize = main::k_MaxInstances * main::k_MaxMeshesPerInstance * sizeof(ShaderInstance);

		MaterialHeapSizes mSizes{};
		mSizes.materialsSize = main::k_MaxInstances * sizeof(ShaderMaterial);

		animationHeap = std::make_unique<AnimationHeap>(k, aSizes);
		drawHeap = std::make_unique<DrawHeap>(k, dSizes);
		materialHeap = std::make_unique<MaterialHeap>(k, mSizes);
		geometryHeap = std::make_unique<GeometryHeap>(k, gSizes);

		textures = std::make_unique<TextureList>(k, *materialHeap, maxTexturesNum);

		auto& allocator = KernelInternal::allocator(k);

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
		{
			geometryTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, GEOM_STAGING_SIZE);
			animationsTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, ANIM_STAGING_SIZE);
			texturesTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, TEX_STAGING_SIZE);

			tracker.emplace(*geometryTBs[i]);
			tracker.emplace(*animationsTBs[i]);
			tracker.emplace(*texturesTBs[i]);
		}
	}

	ResidencyManager::Impl::~Impl() 
	{
		geometryHeap.reset();
		animationHeap.reset();
		materialHeap.reset();
		textures.reset();

		for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i) 
		{
			geometryTBs[i].reset();
			animationsTBs[i].reset();
		}
	}

	void ResidencyManager::Impl::buildClipNames(core::ModelID id, const asset::Model& model)
	{
		auto& clips = clipNames[id];

		clips.clear();

		clips.reserve(model.animations.size());

		for (uint32_t i = 0; i < static_cast<uint32_t>(model.animations.size()); ++i)
		{
			clips.emplace(model.animations[i].name, i);
		}
	}

	void ResidencyManager::Impl::retain(core::ModelID id, const core::ModelRecord& record)
	{
		auto& m = models[id];

		if (m.state == ResidencyState::Retiring) return;

		++m.refCount;

		if (m.state == ResidencyState::Absent && m.refCount == 1)
		{
			m.state = ResidencyState::Allocating;
			++version;
			m.boneCount = static_cast<uint32_t>(record.entry.model.bones.size());
			pending[id] = &record;
		}
	}

	void ResidencyManager::Impl::releaseRetired(uint32_t frameSlot)
	{
		for (auto& retired : retireLists[frameSlot])
		{
			materialHeap->free(retired.materials);
			geometryHeap->free(retired.geometry);
			animationHeap->free(retired.animation);
			
			for (uint32_t slot : retired.textureSlots)
			{
				if (!textures->release(slot)) continue;

				for (auto it = cachedSlots.begin(); it != cachedSlots.end(); )
				{
					if (it->second == slot) it = cachedSlots.erase(it);
					else ++it;
				}
			}


			clipNames.erase(retired.id);
			models.erase(retired.id);
			++version;
		}

		retireLists[frameSlot].clear();
	}

	bool ResidencyManager::Impl::isResident(core::ModelID id) const
	{
		auto it = models.find(id);
		return it != models.end() && it->second.state == ResidencyState::Resident;
	}
	const ResidentModel* ResidencyManager::Impl::find(core::ModelID id) const
	{
		auto it = models.find(id);
		return it == models.end() ? nullptr : &it->second;
	}

	void ResidencyManager::Impl::release(core::ModelID id)
	{
		auto it = models.find(id);
		if (it == models.end()) return;

		auto& m = it->second;
		if (m.refCount == 0) return;
		--m.refCount;

		if (m.refCount != 0) return;

		if (m.state == ResidencyState::Allocating)
		{
			pending.erase(id);
			models.erase(it);
		}
		else if (m.state == ResidencyState::Resident)
		{
			m.state = ResidencyState::Retiring;
			uint32_t slot = directQueue.frameSlot();
			
			RetireEntry retireEntry{};
			retireEntry.id = id;
			retireEntry.materials = m.materials;
			retireEntry.geometry = m.geometry;
			retireEntry.animation = m.animation;
			retireEntry.boneCount = m.boneCount;
			retireEntry.textureSlots = std::move(m.textureSlots);

			retireLists[slot].push_back(retireEntry);
		}
	}

	UploadBatch ResidencyManager::Impl::collectUploads(uint32_t frameSlot)
	{
		UploadBatch batch{};
		if (pending.empty()) return batch;

		auto& geomTB = *geometryTBs[frameSlot];
		auto& animTB = *animationsTBs[frameSlot];
		auto& texTB = *texturesTBs[frameSlot];
		uint8_t* geomBase = reinterpret_cast<uint8_t*>(geomTB.pMappedData());
		uint8_t* animBase = reinterpret_cast<uint8_t*>(animTB.pMappedData());
		uint8_t* texBase = reinterpret_cast<uint8_t*>(texTB.pMappedData());
		uint64_t& geomOffset = geometryOffsets[frameSlot];
		uint64_t& animOffset = animationsOffsets[frameSlot];
		uint64_t& texOffset = texturesOffsets[frameSlot];

		std::vector<core::ModelID> done;
		done.reserve(pending.size());

		for (auto& [id, recordPtr] : pending)
		{
			auto it = models.find(id);
			if (it == models.end() || it->second.state != ResidencyState::Allocating)
			{
				done.push_back(id);
				continue;
			}

			auto& rm = it->second;
			
			const auto& record = *recordPtr;
			const auto& entry = record.entry;

			const bool skinned = !entry.model.skinnedVertices.empty();
			const bool hasAnim = !entry.model.bones.empty();

			const uint64_t st = skinned ? 0 : entry.model.staticVertices.size() * sizeof(asset::StaticVertex);
			const uint64_t sk = skinned ? entry.model.skinnedVertices.size() * sizeof(asset::SkinnedVertex) : 0;
			const uint64_t ix = entry.model.indices.size() * sizeof(uint32_t);
			const uint64_t geoNeed = st + sk + ix;


			AnimationData animData{};
			
			AnimationSizes animSizes{};
			
			uint64_t animNeed = 0;
			
			if (hasAnim)
			{
				animData = buildAnimationData(entry.model, AnimationAllocation{});  // Dummy build animation

				animSizes.boneMetasSize = animData.boneMetas.size() * sizeof(ShaderBoneMeta);
				animSizes.animationsSize = animData.animations.size() * sizeof(ShaderAnimation);
				animSizes.channelsSize = animData.channels.size() * sizeof(ShaderChannel);
				animSizes.keyframesSize = animData.keyframes.size() * sizeof(ShaderKeyframe);
				animSizes.bindPosesSize = animData.bindPoses.size() * sizeof(math::mat4);
				animSizes.inverseBindsSize = animData.inverseBinds.size() * sizeof(math::mat4);

				animNeed = animSizes.animationsSize + animSizes.channelsSize + animSizes.keyframesSize +
					animSizes.bindPosesSize + animSizes.inverseBindsSize + animSizes.boneMetasSize;
			}

			const uint64_t matNeed = entry.model.materials.size() * sizeof(ShaderMaterial);

			uint64_t texNeed = 0;
			{
				std::unordered_set<std::string> seen;
				seen.reserve(entry.model.materials.size() * 4);

				for (const auto& mat : entry.model.materials)
				{
					const char* paths[4] = { mat.albedoPath, mat.normalPath, mat.mrPath, mat.emissivePath };

					for (int k = 0; k < 4; ++k)
					{
						const char* p = paths[k];
						if (!p || !p[0]) continue;   
						if (!seen.insert(p).second) continue; 
						if (cachedSlots.contains(p)) continue; 

						auto fit = record.textures.find(p);
						if (fit == record.textures.end()) continue; 

						for (const auto& mip : fit->second.mips)
						{
							texNeed += mip.size();
						}
					}
				}
			}

			if (texNeed > TEX_STAGING_SIZE)
				throw std::runtime_error("[collectUploads] model textures (" + std::to_string(texNeed) +" bytes) exceed TEX_STAGING_SIZE (" + std::to_string(TEX_STAGING_SIZE) + ")");
			
			if (geomOffset + geoNeed + matNeed > GEOM_STAGING_SIZE) continue;
			if (animOffset + animNeed > ANIM_STAGING_SIZE) continue;
			if (texOffset + texNeed > TEX_STAGING_SIZE) continue;

			GeometryAllocation geom = geometryHeap->allocate(GeometrySizes{ st, sk, ix });
			
			if (!geom.valid(skinned)) 
			{
				geometryHeap->free(geom);
				continue; 
			}

			AnimationAllocation anim{};
			if (hasAnim)
			{
				anim = animationHeap->allocate(animSizes);
				if (!anim.valid())
				{
					animationHeap->free(anim);
					geometryHeap->free(geom);
					continue;
				}
			}

			MaterialAllocation mat = materialHeap->allocate(matNeed);
			
			if (!mat.valid(matNeed))
			{
				materialHeap->free(mat);
				animationHeap->free(anim);
				geometryHeap->free(geom);
				continue;
			}

			if (hasAnim)
			{
				animData = buildAnimationData(entry.model, anim);
			}

			if (geom.staticVB)
			{
				insertToBatch(batch, geomTB, geomBase, geomOffset, geometryHeap->staticPool().buffer(), 
					geom.staticVB->offset, entry.model.staticVertices.data(), geom.staticVB->size);
			}
			if (geom.skinnedVB) 
			{
				insertToBatch(batch, geomTB, geomBase, geomOffset, geometryHeap->skinnedPool().buffer(), 
					geom.skinnedVB->offset, entry.model.skinnedVertices.data(), geom.skinnedVB->size);
			}
			if (geom.index)
			{
				insertToBatch(batch, geomTB, geomBase, geomOffset, geometryHeap->indexedPool().buffer(), 
					geom.index->offset, entry.model.indices.data(), geom.index->size);
			}


			if (anim.animations)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->animationsPool().buffer(),
					anim.animations->offset, animData.animations.data(), animSizes.animationsSize);
			}
			if (anim.channels)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->channelsPool().buffer(),
					anim.channels->offset, animData.channels.data(), animSizes.channelsSize);
			}
			if (anim.keyframes)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->keyframesPool().buffer(),
					anim.keyframes->offset, animData.keyframes.data(), animSizes.keyframesSize);
			}
			if (anim.bindPoses)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->bindPosesPool().buffer(),
					anim.bindPoses->offset, animData.bindPoses.data(), animSizes.bindPosesSize);
			}
			if (anim.inverseBinds)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->inverseBindsPool().buffer(),
					anim.inverseBinds->offset, animData.inverseBinds.data(), animSizes.inverseBindsSize);
			}
			if (anim.boneMetas)
			{
				insertToBatch(batch, animTB, animBase, animOffset, animationHeap->boneMetasPool().buffer(),
					anim.boneMetas->offset, animData.boneMetas.data(), animSizes.boneMetasSize);
			}

			if (!entry.model.materials.empty())
			{
				std::vector<ShaderMaterial> shaderMats;
				shaderMats.reserve(entry.model.materials.size() + 1);

				std::unordered_set<uint32_t> releasedSlots;
				std::vector<uint32_t> modelSlots;

				for (const auto& mat : entry.model.materials)
				{
					ShaderMaterial sm{};
					sm.baseColor = mat.baseColor;
					sm.metallic = mat.metallic;
					sm.roughness = mat.roughness;
					sm.albedoIdx = resolveRetain(record, mat.albedoPath, batch, texTB, texBase, texOffset, releasedSlots, modelSlots);
					sm.normalIdx = resolveRetain(record, mat.normalPath, batch, texTB, texBase, texOffset, releasedSlots, modelSlots);
					sm.mrIdx = resolveRetain(record, mat.mrPath, batch, texTB, texBase, texOffset, releasedSlots, modelSlots);
					sm.emissiveIdx = resolveRetain(record, mat.emissivePath, batch, texTB, texBase, texOffset, releasedSlots, modelSlots);
					shaderMats.push_back(sm);
				}

				if (mat.materials)
				{
					insertToBatch(batch, geomTB, geomBase, geomOffset, materialHeap->pool().buffer(), 
						mat.materials->offset, shaderMats.data(), shaderMats.size() * sizeof(ShaderMaterial));
				}

				rm.textureSlots = std::move(modelSlots);
			}

			rm.geometry = geom;
			rm.animation = anim;
			rm.materials = mat;           
			rm.boneCount = (!entry.model.skinnedVertices.empty()) ? static_cast<uint32_t>(entry.model.bones.size()) : 0;
			rm.state = ResidencyState::Resident;
			buildClipNames(id, entry.model);
			++version;
			done.push_back(id);
		}

		for (auto id : done) pending.erase(id);
		return batch;
	}

	std::optional<uint32_t> ResidencyManager::Impl::clipIndex(core::ModelID id, std::string_view name) const
	{
		auto mit = models.find(id);
		if (mit == models.end() || mit->second.state != ResidencyState::Resident)
			return std::nullopt;

		auto cit = clipNames.find(id);
		if (cit == clipNames.end()) return std::nullopt;

		auto it = cit->second.find(std::string(name));
		if (it == cit->second.end()) return std::nullopt;
		return it->second;
	}

	void ResidencyManager::Impl::resetStaging(uint32_t slot)
	{
		geometryOffsets[slot] = 0;
		animationsOffsets[slot] = 0;
		texturesOffsets[slot] = 0;
	}

	uint32_t ResidencyManager::Impl::insertTexture(const std::string& path,const asset::BcTexture& texture, UploadBatch& batch,
		vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset)
	{
		if (auto it = cachedSlots.find(path); it != cachedSlots.end())
			return it->second;

		TextureDesc desc
		{
			.fmt = static_cast<vhl::Format>(texture.header.format),
			.width = texture.header.width,
			.height = texture.header.height,
			.mipLevels = texture.header.mipLevels,
		};

		Texture tex = createTexture(device, allocator, desc);

		for (uint32_t mip = 0; mip < texture.header.mipLevels; ++mip)
		{
			const uint32_t w = std::max(1U, texture.header.width >> mip);
			const uint32_t h = std::max(1U, texture.header.height >> mip);
			const uint8_t* mipData = texture.mips[mip].data();
			const uint64_t sz = texture.mips[mip].size();

			insertToBatch(batch, texTB, texBase, texOffset, *tex.image,
				mip, 0, w, h, 1, mipData, sz);
		}

		const uint32_t slot = textures->add(std::move(tex));
		cachedSlots[path] = slot;
		return slot;
	}

	uint32_t ResidencyManager::Impl::resolveTexture(const core::ModelRecord& record, const char* path, UploadBatch& batch, vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset)
	{
		if (!path || !path[0]) return UINT32_MAX;
		
		auto it = record.textures.find(record.entry.path + path);
		if (it == record.textures.end()) return UINT32_MAX;

		return insertTexture(record.entry.path + it->first, it->second, batch, texTB, texBase, texOffset);
	}

	uint32_t ResidencyManager::Impl::resolveRetain(const core::ModelRecord& record, const char* path, UploadBatch& batch, vhl::StagingBuffer& texTB, uint8_t* texBase, uint64_t& texOffset, std::unordered_set<uint32_t>& retainedSlots, std::vector<uint32_t>& modelSlots)
	{
		const uint32_t slot = resolveTexture(record, path, batch, texTB, texBase, texOffset);

		if (slot == UINT32_MAX) return UINT32_MAX;

		if (retainedSlots.insert(slot).second)
		{
			textures->retain(slot);
			modelSlots.push_back(slot);
		}

		return slot;
	}

	ResidencyManager::~ResidencyManager() = default;

	ResidencyManager::ResidencyManager(RenderKernel& kernel, uint32_t maxTexturesNum)
		: m_impl(new Impl(kernel, maxTexturesNum))
	{
	}

	void ResidencyManager::retain(core::ModelID id, const core::ModelRecord& record)
	{
		m_impl->retain(id, record);
	}

	void ResidencyManager::release(core::ModelID id)
	{
		m_impl->release(id);
	}

	std::optional<uint32_t> ResidencyManager::clipIndex(core::ModelID id, std::string_view name) const
	{
		return m_impl->clipIndex(id, name);
	}

	bool ResidencyManager::isResident(core::ModelID id) const
	{
		return m_impl->isResident(id);
	}

	const ResidentModel* ResidencyManager::find(core::ModelID id) const
	{
		return m_impl->find(id);
	}

	uint64_t ResidencyManager::version() const
	{
		return m_impl->version;
	}


	void ResidencyInternal::resetStaging(ResidencyManager& rm, uint32_t slot)
	{
		rm.m_impl->resetStaging(slot);
	}

	void ResidencyInternal::releaseRetired(ResidencyManager& rm, uint32_t slot)
	{
		rm.m_impl->releaseRetired(slot);
	}

	UploadBatch ResidencyInternal::collectUploads(ResidencyManager& rm, uint32_t slot)
	{
		return rm.m_impl->collectUploads(slot);
	}

	TextureList& ResidencyInternal::textures(ResidencyManager& rm)
	{
		return *rm.m_impl->textures;
	}

	MaterialHeap& ResidencyInternal::matHeap(ResidencyManager& rm)
	{
		return *rm.m_impl->materialHeap;
	}

	GeometryHeap& ResidencyInternal::geomHeap(ResidencyManager& rm)
	{
		return *rm.m_impl->geometryHeap;
	}

	AnimationHeap& ResidencyInternal::animHeap(ResidencyManager& rm)
	{
		return *rm.m_impl->animationHeap;
	}

	DrawHeap& ResidencyInternal::drawHeap(ResidencyManager& rm)
	{
		return *rm.m_impl->drawHeap;
	}
}