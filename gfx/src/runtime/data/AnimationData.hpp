#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <core/Scene.hpp>

namespace tungol::runtime
{
	// ================================= GPU TYPES =============================
	
	struct ShaderChannel
	{
		uint32_t boneIndex = 0;
		uint32_t keyCount = 0;
		uint32_t keyIndex = 0;
	};

	struct ShaderAnimation
	{
		float duration = 0.0f;
		uint32_t channelIndex = 0;
		uint32_t channelCount = 0;
	};

	struct ShaderKeyframe
	{
		float time;

		alignas(16) math::vec3 position;
		alignas(16) math::quat rotation;
		alignas(16) math::vec3 scale;
	};

	struct ShaderBoneMeta
	{
		uint32_t parentIndex = UINT32_MAX;
		uint32_t inverseBindIndex = 0;
		uint32_t bindPoseIndex = 0;
		uint32_t localBoneIndex = 0;
	};

	struct AnimationData
	{
		std::vector<ShaderAnimation> animations;
		std::vector<ShaderChannel> channels;
		std::vector<ShaderKeyframe> keyframes;
		std::vector<ShaderBoneMeta> boneMetas;

		std::vector<math::mat4> bindPoses;
		std::vector<math::mat4> inverseBinds;
	};

	TUNGOL_EXPORT AnimationData buildAnimationData(const asset::Model& model, const struct AnimationAllocation& ranges);
}