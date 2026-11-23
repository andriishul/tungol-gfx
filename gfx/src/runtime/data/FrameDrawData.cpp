#include "FrameDrawData.hpp"
#include <runtime/RenderConfig.hpp>
#include <runtime/ResidencyManager.hpp>
#include <vhl/commands/DrawIndirectCommand.hpp>

#include "AnimationData.hpp"
#include "MaterialData.hpp"

namespace tungol::runtime
{
    std::vector<AnimRef> buildAnimRefs(const core::Scene& scene, const core::AssetCache& assets, const ResidencyManager& residency)
    {
        const auto& instances = scene.instances();
        std::vector<AnimRef> out(instances.size());

        uint32_t currentSequentialBoneOffset = 0;

        for (uint32_t i = 0; i < instances.size(); ++i)
        {
            const auto& instance = instances[i];
            const ResidentModel* rm = residency.find(instance.modelID);

            if (!rm || rm->state != ResidencyState::Resident)
            {
                out[i] = AnimRef{};
                out[i].clipIndex = UINT32_MAX;
                continue;
            }

            out[i].boneOffset = currentSequentialBoneOffset;
            out[i].boneCount = rm->boneCount;
            currentSequentialBoneOffset += rm->boneCount;

            out[i].boneMetaOffset = rm->animation.boneMetas.has_value()
                ? static_cast<uint32_t>(rm->animation.boneMetas->offset / sizeof(ShaderBoneMeta)) : 0;

            auto clipIndexOpt = residency.clipIndex(instance.modelID, instance.clipName);

            if (clipIndexOpt.has_value() && rm->animation.animations.has_value())
            {
                uint32_t animBase = static_cast<uint32_t>(rm->animation.animations->offset / sizeof(ShaderAnimation));
                out[i].clipIndex = *clipIndexOpt + animBase;
            }
            else out[i].clipIndex = UINT32_MAX;
        }
        return out;
    }

    void applyAnimRefs(const std::vector<AnimRef>& animRefs, const core::Scene& scene,
        const ResidencyManager& residency, FrameDrawData& out)
    {
        const auto& instances = scene.instances();
        size_t pbIdx = 0;

        for (size_t i = 0; i < instances.size() && pbIdx < out.playbacks.size(); ++i)
        {
            const auto& inst = instances[i];
            const ResidentModel* rm = residency.find(inst.modelID);
            if (!rm || rm->state != ResidencyState::Resident || rm->boneCount == 0)
                continue;

            if (i < animRefs.size())
            {
                out.playbacks[pbIdx].clipIndex = animRefs[i].clipIndex;
                out.playbacks[pbIdx].boneOffset = animRefs[i].boneOffset;
                out.playbacks[pbIdx].boneMetaOffset = animRefs[i].boneMetaOffset;
            }

            ++pbIdx;
        }
    }

    void buildDrawData(const SceneSnapshot& input, FrameDrawData& out, const ResidencyManager& residency)
    {
        const auto& instances = input.scene.instances();

        out.instances.clear();
        out.playbacks.clear();
        out.indirectCmds.clear();

        out.instances.reserve(instances.size() * main::k_MaxMeshesPerInstance);
        out.instanceSceneIndices.reserve(instances.size() * main::k_MaxMeshesPerInstance);
        out.playbacks.reserve(instances.size());

        std::vector<vhl::DrawIndexedIndirectCommand> staticCmds, skinnedCmds;

        uint32_t row = 0;

        for (uint32_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];
            const ResidentModel* rm = residency.find(inst.modelID);
            if (!rm || rm->state != ResidencyState::Resident) continue;

            uint32_t materialBase = 0;
            if (rm->materials.materials.has_value())
            {
                materialBase = static_cast<uint32_t>(
                    rm->materials.materials->offset / sizeof(ShaderMaterial));
            }

            const bool active = inst.active;
            const auto& model = input.assets.record(inst.modelID).entry.model;
            const uint32_t paletteOffset = input.animRefs[i].boneOffset;
            const uint32_t clipIndex = input.animRefs[i].clipIndex;

            if (rm->boneCount > 0)
            {
                ShaderPlaybackState pb{};
                pb.time = inst.time;
                pb.clipIndex = clipIndex;
                pb.boneOffset = paletteOffset;
                pb.boneCount = rm->boneCount;
                pb.boneMetaOffset = input.animRefs[i].boneMetaOffset;
                out.playbacks.push_back(pb);
            }

            if (out.playbacks.size() > main::k_MaxPlaybackCount)
                throw std::runtime_error("[buildDrawData] playback buffer overflow");

            if (rm->boneCount > main::k_MaxBonesPerInstance)
                throw std::runtime_error("[buildDrawData] bone cap exceeded");

            if (model.meshes.size() > main::k_MaxMeshesPerInstance)
                throw std::runtime_error("[buildDrawData] mesh cap exceeded");

            for (const auto& mesh : model.meshes)
            {
                bool isSkinned = mesh.hasSkin && rm->boneCount > 0;

                ShaderInstance si{};
                si.worldTransform = inst.worldTransform;
                si.materialIndex = model.materials.empty() ? UINT32_MAX : materialBase + mesh.materialIndex;
                si.boneOffset = isSkinned ? paletteOffset : UINT32_MAX;
                out.instances.push_back(si);
                out.instanceSceneIndices.push_back(i);

                vhl::DrawIndexedIndirectCommand cmd{};
                cmd.indexCount = mesh.indexCount;
                cmd.instanceCount = active ? 1 : 0;

                if (rm->geometry.index.has_value())
                    cmd.firstIndex = static_cast<uint32_t>(rm->geometry.index->offset / 4) + mesh.firstIndex;

                if (isSkinned && rm->geometry.skinnedVB.has_value())
                    cmd.vertexOffset = static_cast<int32_t>(rm->geometry.skinnedVB->offset / sizeof(asset::SkinnedVertex)) + mesh.vertexIndex;
                else if (!isSkinned && rm->geometry.staticVB.has_value())
                    cmd.vertexOffset = static_cast<int32_t>(rm->geometry.staticVB->offset / sizeof(asset::StaticVertex)) + mesh.vertexIndex;

                cmd.firstInstance = row;
                (isSkinned ? skinnedCmds : staticCmds).push_back(cmd);
                ++row;
            }
        }

        out.staticIndirectCount = static_cast<uint32_t>(staticCmds.size());
        out.skinnedIndirectCount = static_cast<uint32_t>(skinnedCmds.size());

        if (out.instances.size() > main::k_MaxInstances * main::k_MaxMeshesPerInstance)
            throw std::runtime_error("[buildDrawData] instance buffer overflow");

        if (out.staticIndirectCount + out.skinnedIndirectCount > main::k_MaxInstances * main::k_MaxMeshesPerInstance)
            throw std::runtime_error("[buildDrawData] indirect buffer overflow");

        out.indirectCmds.reserve(staticCmds.size() + skinnedCmds.size());
        out.indirectCmds.insert(out.indirectCmds.end(), staticCmds.begin(), staticCmds.end());
        out.indirectCmds.insert(out.indirectCmds.end(), skinnedCmds.begin(), skinnedCmds.end());
    }

    uint32_t totalBoneCount(const FrameDrawData& fd)
    {
        uint32_t total = 0;
        for (const auto& pb : fd.playbacks)
            total += pb.boneCount;
        return total;
    }
}