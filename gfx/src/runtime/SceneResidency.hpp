#pragma once
#include <runtime/ResidencyManager.hpp>
#include "data/FrameDrawData.hpp"
#include <unordered_set>

namespace tungol::runtime 
{
    struct TUNGOL_EXPORT SceneResidency
    {
        void updateResidency(ResidencyManager& residency, const core::Scene& scene, const core::AssetCache& assets);
        void updateState(const ResidencyManager& residency, const core::Scene& scene, FrameDrawData& drawData);


        const std::unordered_set<core::ModelID>& needed() const { return m_needed; }
        const std::unordered_set<core::ModelID>& held() const { return m_held; }

        void markHeld(core::ModelID id) { m_held.insert(id); }
        void markReleased(core::ModelID id) { m_held.erase(id); }
    private:
        std::unordered_set<core::ModelID> m_needed, m_held;
        uint64_t m_lastLayoutVersion = UINT64_MAX;
    };
}