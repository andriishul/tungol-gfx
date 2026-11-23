#include "AnimationData.hpp"
#include <runtime/heaps/AnimationHeap.hpp>

namespace tungol::runtime
{
    AnimationData buildAnimationData(const asset::Model& model, const AnimationAllocation& ranges)
    {
        AnimationData out{};

        if (model.skinnedVertices.empty()) return out;

        const uint32_t channelBase = ranges.channels.has_value()
            ? static_cast<uint32_t>(ranges.channels->offset / sizeof(ShaderChannel)) : 0;
        const uint32_t keyframeBase = ranges.keyframes.has_value()
            ? static_cast<uint32_t>(ranges.keyframes->offset / sizeof(ShaderKeyframe)) : 0;
        const uint32_t boneMetaBase = ranges.boneMetas.has_value()
            ? static_cast<uint32_t>(ranges.boneMetas->offset / sizeof(ShaderBoneMeta)) : 0;
        const uint32_t inverseBindBase = ranges.inverseBinds.has_value()
            ? static_cast<uint32_t>(ranges.inverseBinds->offset / sizeof(math::mat4)) : 0;
        const uint32_t bindPoseBase = ranges.bindPoses.has_value()
            ? static_cast<uint32_t>(ranges.bindPoses->offset / sizeof(math::mat4)) : 0;

        // 1. BONES
        out.boneMetas.reserve(model.bones.size());
        for (uint32_t i = 0; i < model.bones.size(); ++i)
        {
            const auto& bone = model.bones[i];
            ShaderBoneMeta meta{};

            // PREVENT INFINITE LOOPS (Fixes the shattering)
            meta.parentIndex = (bone.parentIndex == UINT32_MAX || bone.parentIndex == -1 || bone.parentIndex == i)
                ? UINT32_MAX
                : (bone.parentIndex + boneMetaBase);

            meta.inverseBindIndex = inverseBindBase + i;
            meta.bindPoseIndex = bindPoseBase + i;
            meta.localBoneIndex = i;

            out.boneMetas.push_back(meta);
        }

        // 2. ANIMATIONS
        out.animations.reserve(model.animations.size());
        for (const auto& src : model.animations)
        {
            ShaderAnimation a{};
            a.duration = src.duration;
            a.channelIndex = src.channelIndex + channelBase;
            a.channelCount = src.channelCount;
            out.animations.push_back(a);
        }

        // 3. CHANNELS
        out.channels.reserve(model.channels.size());
        for (const auto& src : model.channels)
        {
            ShaderChannel c{};
            c.boneIndex = src.boneIndex;
            c.keyCount = src.keyCount;
            c.keyIndex = src.keyIndex + keyframeBase;
            out.channels.push_back(c);
        }

        // 4. KEYFRAMES
        out.keyframes.reserve(model.frames.size());
        for (const auto& src : model.frames)
        {
            ShaderKeyframe kf{};
            kf.time = src.time;
            kf.position = src.position;
            kf.rotation = src.rotation;
            kf.scale = src.scale;
            out.keyframes.push_back(kf);
        }

        // 5. BIND POSES & INVERSE BINDS
        out.bindPoses.reserve(model.bones.size());
        out.inverseBinds.reserve(model.bones.size());
        for (const auto& bone : model.bones)
        {
            out.bindPoses.push_back(bone.bindLocalTransform);
            out.inverseBinds.push_back(bone.inverseBindMatrix);
        }

        return out;
    }
}