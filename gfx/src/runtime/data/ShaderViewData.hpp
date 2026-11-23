#pragma once
#include "ShadowData.hpp"

namespace tungol::runtime 
{
	struct alignas(16) ShaderCameraData
	{
		math::mat4 projection;
		math::mat4 view;
		math::vec3 position;
	};

	struct alignas(16) ShaderLightData
	{
		math::vec3 direction;
		float intensity;
		math::vec3 color;
	};

	struct alignas(16) ShaderShadowData
	{
		math::mat4 viewProj;
		float bias;
	};

	struct ShaderViewData
	{
		ShaderCameraData camera;
		ShaderLightData light;
		ShaderShadowData shadow;
	};

	ShaderViewData buildShaderViewData(const core::Scene& scene, const core::ViewData& data, const std::optional<asset::AABB>& aabb)
	{
		ShaderCameraData cam
		{
			.projection = data.camera.projection,
			.view = data.camera.view,
			.position = data.camera.position
		};


		ShaderLightData light
		{
			.direction = data.light.direction,
			.intensity = data.light.intensity,
			.color = data.light.color
		};


		ShaderShadowData shadow
		{
			.bias = data.shadow.bias
		};

		if (aabb.has_value()) 
		{
			shadow.viewProj = computeShadowViewProj(*aabb, light.direction);
		}

		ShaderViewData svd{};
		svd.camera = cam;
		svd.light = light;
		svd.shadow = shadow;

		return svd;
	}

}