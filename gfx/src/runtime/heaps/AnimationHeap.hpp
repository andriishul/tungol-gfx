#pragma once
#include "Pool.hpp"

namespace tungol::runtime
{
    struct AnimationHeapSizes
    {
        uint64_t boneMetasSize;
        uint64_t animationsSize;
        uint64_t channelsSize;
        uint64_t keyframesSize;
        uint64_t bindPosesSize;
        uint64_t inverseBindsSize;

        uint64_t playbackSize;
        uint64_t boneLocalsSize;
        uint64_t bonePaletteSize;
    };

    struct AnimationSizes
    {
        uint64_t boneMetasSize;
        uint64_t animationsSize;
        uint64_t channelsSize;
        uint64_t keyframesSize;
        uint64_t bindPosesSize;
        uint64_t inverseBindsSize;
    };

    struct AnimationAllocation
    {
        std::optional<Range> boneMetas, animations, channels, keyframes, bindPoses, inverseBinds;

        bool valid() const
        {
            const bool any = boneMetas || animations || channels || keyframes || bindPoses || inverseBinds;
            if (!any) return true;
            return boneMetas && animations && channels && keyframes && bindPoses && inverseBinds;
        }
    };

    class TUNGOL_EXPORT AnimationHeap
    {
    public:
        AnimationHeap(RenderKernel& kernel, const AnimationHeapSizes& sizes);
        ~AnimationHeap();

        AnimationAllocation allocate(const AnimationSizes& sizes);
        void free(const AnimationAllocation& a);

        const Pool& boneMetasPool() const;
        const Pool& animationsPool() const;
        const Pool& channelsPool() const;
        const Pool& keyframesPool() const;
        const Pool& bindPosesPool() const;
        const Pool& inverseBindsPool() const;

        vhl::Buffer& playbackSBs(uint32_t slot) const;
        vhl::Buffer& playbackTBs(uint32_t slot) const;
        vhl::Buffer& boneLocalsSBs(uint32_t slot) const;
        vhl::Buffer& bonePaletteSBs(uint32_t slot) const;

        const vhl::DescriptorSetHandle& sampleSets(uint32_t slot) const;
        const vhl::DescriptorSetHandle& hierarchySets(uint32_t slot) const;

        AnimationHeapSizes sizes() const;
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}