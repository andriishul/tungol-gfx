#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>
#include <core/Scene.hpp>

namespace tungol::runtime
{
	class TUNGOL_EXPORT AnimationResources
	{
	public:
		AnimationResources(RenderKernel&);
		~AnimationResources();
    private:
        void buildStatic(const core::Scene& scene, const core::ModelRegistry& models, vhl::CopyContext& cpy);
        uint8_t* buildPlayback(const core::Scene& scene, const core::ModelRegistry& models);
    public:
        uint64_t playbackSize() const;

        vhl::Buffer& boneMetaSB() const;
        vhl::Buffer& animationsSB() const;
        vhl::Buffer& channelsSB() const;
        vhl::Buffer& keyframesSB() const;
        vhl::Buffer& bindPosesSB() const;
        vhl::Buffer& inverseBindsSB() const;

        vhl::Buffer& playbackSBs(uint32_t slot) const;
        vhl::Buffer& playbackTBs(uint32_t slot) const;
        vhl::Buffer& boneLocalsSBs(uint32_t slot) const;
        vhl::Buffer& bonePaletteSBs(uint32_t slot) const;

        const uint32_t boneCount() const;
        const uint32_t boneOffset(uint32_t instanceIndex);

        const vhl::DescriptorSetHandle& sampleSets(uint32_t slot) const;
        const vhl::DescriptorSetHandle& hierarchySets(uint32_t slot) const;
	private:
        friend class RenderScene;

		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
