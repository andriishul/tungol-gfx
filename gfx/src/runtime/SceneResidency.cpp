#include "SceneResidency.hpp"

#include <chrono>

namespace tungol::runtime 
{
	void SceneResidency::updateResidency(ResidencyManager& residency, const core::Scene& scene, const core::AssetCache& assets)
	{
		//auto t0 = std::chrono::high_resolution_clock::now();

		const uint64_t lv = scene.layoutVersion();
		
		if (lv != m_lastLayoutVersion)
		{
			m_needed.clear();
			for (const auto& inst : scene.instances()) 
			{
				if (inst.active && inst.modelID != core::EMPTY_MODEL)
					m_needed.insert(inst.modelID);
			}
			m_lastLayoutVersion = lv;
		}

		for (core::ModelID id : m_needed)
		{
			if (m_held.contains(id)) continue;
			if (!assets.isReady(id)) continue;
			residency.retain(id, assets.record(id));
			m_held.insert(id);
		}

		for (auto it = m_held.begin(); it != m_held.end();)
		{
			if (m_needed.contains(*it)) 
			{
				++it; 
				continue; 
			}
			residency.release(*it);
			it = m_held.erase(it);
		}

		//auto t1 = std::chrono::high_resolution_clock::now();

		//auto elapsed = std::chrono::duration<float, std::milli>(t1 - t0).count();
		//printf("updateResidency: %.2f ms (sv=%llu, cached=%llu, needed=%zu, held=%zu)\n",
		//	elapsed, sv, m_lastVersion, m_needed.size(), m_held.size());
	}
	
	void SceneResidency::updateState(const ResidencyManager& residency, const core::Scene& scene, FrameDrawData& drawData)
	{
		if (drawData.instances.empty() || drawData.instanceSceneIndices.empty())
			return;

		const auto& instances = scene.instances();

		const size_t count = std::min(drawData.instances.size(), drawData.instanceSceneIndices.size());
		for (size_t idx = 0; idx < count; ++idx)
		{
			const uint32_t sceneIdx = drawData.instanceSceneIndices[idx];
			if (sceneIdx < instances.size())
			{
				drawData.instances[idx].worldTransform = instances[sceneIdx].worldTransform;
			}
		}

		size_t pbIdx = 0;
		for (size_t i = 0; i < instances.size() && pbIdx < drawData.playbacks.size(); ++i)
		{
			const auto& inst = instances[i];
			const ResidentModel* rm = residency.find(inst.modelID);
			
			if (!rm || rm->state != ResidencyState::Resident) continue;

			if (rm->boneCount > 0)
			{
				drawData.playbacks[pbIdx++].time = inst.time;
			}
		}
	}

}