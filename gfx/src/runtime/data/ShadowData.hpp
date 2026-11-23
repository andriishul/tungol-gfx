#pragma once
#include <core/Scene.hpp>
#include <math/vm.hpp>

namespace tungol::runtime
{
    inline bool computeSceneAABB(const core::Scene& scene, const core::AssetCache& assets, asset::AABB& aabb)
    {
        aabb.min = { +1e30f, +1e30f, +1e30f };
        aabb.max = { -1e30f, -1e30f, -1e30f };

        bool computed = false;

        for (const auto& inst : scene.instances())
        {
            if (!inst.active) continue;
            if (!assets.isReady(inst.modelID)) continue;

            const auto& rec = assets.record(inst.modelID);
            const math::mat4& mtx = inst.worldTransform;

            const asset::AABB& local = rec.entry.model.localAABB;

            for (int i = 0; i < 8; ++i)
            {
                const math::vec3 corner =
                {
                    (i & 1) ? local.max.x : local.min.x,
                    (i & 2) ? local.max.y : local.min.y,
                    (i & 4) ? local.max.z : local.min.z,
                };

                const math::vec4 w = mtx * math::vec4(corner.x, corner.y, corner.z, 1.0f);

                aabb.min.x = std::min(aabb.min.x, w.x);
                aabb.min.y = std::min(aabb.min.y, w.y);
                aabb.min.z = std::min(aabb.min.z, w.z);

                aabb.max.x = std::max(aabb.max.x, w.x);
                aabb.max.y = std::max(aabb.max.y, w.y);
                aabb.max.z = std::max(aabb.max.z, w.z);
            }

            computed = true;
        }

        if (!computed || aabb.min.x > aabb.max.x)
        {
            aabb.min = { -1.0f, -1.0f, -1.0f };
            aabb.max = { 1.0f,  1.0f,  1.0f };
            return false;
        }

        const math::vec3 center = (aabb.min + aabb.max) * 0.5f;
        const math::vec3 half = (aabb.max - aabb.min) * 0.5f * 1.2f;
        aabb.min = center - half;
        aabb.max = center + half;

        return true;
    }

    // Fits an orthographic shadow camera around the scene AABB, oriented toward the light.
    // Camera-independent: result depends only on scene bounds and light direction.
    inline math::mat4 computeShadowViewProj(const asset::AABB& aabb, math::vec3 lightDirection)
    {
        const auto aabbMin = aabb.min;
        const auto aabbMax = aabb.max;

        // Preconditions — fail loud, not silent.
        if (aabbMin.x > aabbMax.x || aabbMin.y > aabbMax.y || aabbMin.z > aabbMax.z)
        {
            throw std::runtime_error("[computeShadowViewProj] Invalid scene AABB (min > max). Scene bounds not computed was commit() called?");
        }

        const math::vec3 toLightRaw = { -lightDirection.x, -lightDirection.y, -lightDirection.z };
        const float toLightLen = math::length(toLightRaw);

        if (toLightLen < 1e-6f)
        {
            throw std::runtime_error("[computeShadowViewProj] Zero-length light direction. FrameData::light.direction was not set.");
        }

        // Fit.
        const math::vec3 center = (aabbMin + aabbMax) * 0.5f;
        float radius = math::length(aabbMax - aabbMin) * 0.5f;
        if (radius < 1e-4f) radius = 1.0f;   // scene smaller than 0.1mm → use unit box

        const math::vec3 toLight = toLightRaw / toLightLen;

        const math::vec3 up = (std::abs(toLight.y) > 0.99f)
            ? math::vec3(0.0f, 0.0f, 1.0f)
            : math::vec3(0.0f, 1.0f, 0.0f);

        const math::vec3 lightPos = center + toLight * radius * 2.0f;
        const math::mat4 lightView = math::lookAt(lightPos, center, up);
        const math::mat4 lightProj = math::ortho(-radius, radius, -radius, radius, 0.0f, radius * 4.0f);

        return lightProj * lightView;
    }
}
