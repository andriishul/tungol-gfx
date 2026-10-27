#pragma once
#include "ModelRegistry.hpp"

namespace tungol::core
{
	struct Instance 
	{
		ModelID modelID = EMPTY_MODEL;
		math::mat4 transform{};

		std::string clipName; // Make sure that the model with this modelID has the exact clipName.
		
		float time = 0.0f; // Should be updated using deltaTime for animated objects
	};

	class Scene 
	{
	public:
		Scene(const std::vector<Instance> instances) : m_instances(instances) {}
		
		Instance& instance(uint32_t idx) { return m_instances[idx]; }

		const std::vector<Instance>& instances() const { return m_instances; }
	private:
		std::vector<Instance> m_instances;
	};


	class SceneBuilder final
	{
	public:
		explicit SceneBuilder(const ModelRegistry& models) : m_models(models) {}

		uint32_t addInstance(ModelID id, const math::mat4& transform = math::mat4::identity(), const std::string& clipName = {}, float time = 0.0f)
		{
			const auto& model = m_models.model(id);

			uint32_t idx = static_cast<uint32_t>(m_instances.size());

			m_instances.push_back(Instance{ id, transform, clipName, time });

			return idx;
		}

		Scene build() const 
		{
			return Scene(std::move(m_instances));
		}

	private:
		const ModelRegistry& m_models;
		std::vector<Instance> m_instances;
	};
}
