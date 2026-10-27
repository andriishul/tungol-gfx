#pragma once
#include "AnimationResources.hpp"
#include "ShadowResources.hpp"
#include "Texture.hpp"

#include <core/ViewData.hpp>

namespace tungol::runtime
{
	class TUNGOL_EXPORT RenderScene 
	{
	public:
		RenderScene(RenderKernel& kernel, core::ModelRegistry& models, const core::Scene& scene);
		~RenderScene();

		const core::Scene& scene() const;
		core::Scene& scene();
	private:
		void commit(const core::Scene& scene);

		uint32_t staticDrawCount() const;
		uint32_t skinnedDrawCount() const;

		const vhl::DescriptorSetHandle& objectSets(uint32_t slot) const;

		vhl::Buffer& staticVB() const;
		vhl::Buffer& skinnedVB() const;
		vhl::Buffer& indexBuf() const;
		vhl::Buffer& indirectBuf() const;

		vhl::Buffer& materialSB() const;
		vhl::Buffer& instanceSBs(uint32_t slot) const;
		vhl::Buffer& instanceTBs(uint32_t slot) const;

		TextureRegistry& textures() const;

		uint8_t* build(const core::Scene& scene) const;
		uint8_t* buildPlayback(const core::Scene& scene) const;

		const AnimationResources& animations() const;
		const ShadowResources& shadows() const;

		math::vec3 sceneMin() const;
		math::vec3 sceneMax() const;
	private:
		friend class Renderer;
		struct Impl;
		std::unique_ptr<Impl> m_impl;
		core::Scene m_scene;
	};
}
