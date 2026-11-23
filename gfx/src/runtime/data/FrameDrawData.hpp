#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>
#include <core/Scene.hpp>

namespace tungol::runtime
{
#pragma region Types

	struct AnimRef
	{
		uint32_t boneOffset = UINT32_MAX;
		uint32_t clipIndex = UINT32_MAX;
		uint32_t boneCount = 0;
		uint32_t boneMetaOffset = 0;
	};

	// =============================== GPU TYPES ======================================

	struct alignas(16) ShaderInstance
	{
		math::mat4 worldTransform;
		uint32_t materialIndex = UINT32_MAX;
		uint32_t boneOffset = UINT32_MAX;
	};

	struct ShaderPlaybackState
	{
		float time = 0.0f;
		uint32_t clipIndex = UINT32_MAX;
		uint32_t boneOffset = UINT32_MAX;
		uint32_t boneCount = 0;
		uint32_t boneMetaOffset = 0;
	};

	// ========================================================================================

	struct SceneSnapshot
	{
		const core::Scene& scene;
		const core::AssetCache& assets;
		const std::vector<AnimRef>& animRefs;
	};

	struct FrameDrawData
	{
		std::vector<vhl::DrawIndexedIndirectCommand> indirectCmds;

		uint32_t staticIndirectCount = 0;
		uint32_t skinnedIndirectCount = 0;

		std::vector<ShaderPlaybackState> playbacks;
		
		std::vector<ShaderInstance> instances;
		std::vector<uint32_t> instanceSceneIndices;
	};
#pragma endregion

	TUNGOL_EXPORT std::vector<AnimRef> buildAnimRefs(const core::Scene& scene, const core::AssetCache& assets, const ResidencyManager& residency);
	
	TUNGOL_EXPORT void applyAnimRefs(const std::vector<AnimRef>& animRefs, const core::Scene& scene, const ResidencyManager& residency, FrameDrawData& out);
	TUNGOL_EXPORT void buildDrawData(const SceneSnapshot& input, FrameDrawData& out, const ResidencyManager& residency);

	TUNGOL_EXPORT uint32_t totalBoneCount(const FrameDrawData& fd);
}