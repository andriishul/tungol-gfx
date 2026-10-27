#pragma once
#include <math/vm.hpp>

namespace tungol 
{

    // Fits an orthographic shadow camera around the scene AABB, oriented toward the light.
    // Camera-independent: result depends only on scene bounds and light direction.
    inline math::mat4 computeShadowViewProj(math::vec3 sceneMin, math::vec3 sceneMax, math::vec3 lightDirection)
    {
        // Preconditions — fail loud, not silent.
        if (sceneMin.x > sceneMax.x || sceneMin.y > sceneMax.y || sceneMin.z > sceneMax.z)
        {
            throw std::runtime_error(
                "[computeShadowViewProj] Invalid scene AABB (min > max). "
                "Scene bounds not computed — was commit() called?");
        }

        const math::vec3 toLightRaw = { -lightDirection.x, -lightDirection.y, -lightDirection.z };
        const float toLightLen = math::length(toLightRaw);

        if (toLightLen < 1e-6f)
        {
            throw std::runtime_error(
                "[computeShadowViewProj] Zero-length light direction. "
                "FrameData::light.direction was not set.");
        }

        // Fit.
        const math::vec3 center = (sceneMin + sceneMax) * 0.5f;
        float radius = math::length(sceneMax - sceneMin) * 0.5f;
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
