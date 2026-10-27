#pragma once
#include <math/types.hpp>

namespace tungol::core
{
	using ClearColorValue = std::array<float, 4>;

	struct Camera
	{
		math::mat4 projection;
		math::mat4 view;
		math::vec3 position;
	};

	struct Light
	{
		math::vec3 direction;
		float intensity;
		math::vec3 color;
	};

	struct Shadow
	{
		float bias = 0.002f;
	};

	struct Viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};

	struct Scissor
	{
		int32_t x = 0;
		int32_t y = 0;
		uint32_t width = 0;
		uint32_t height = 0;
	};


	struct ViewData
	{
		Camera camera;
		Light light;
		Shadow shadow;

		ClearColorValue clearColor = { 0.5f, 0.7f, 1.0f, 1.0f };

		// Optional. If unset, the renderer uses the full framebuffer size for this scene.
		// If set, you take over and must resize them yourself when the window/framebuffer changes.
		std::optional<Viewport> viewport;
		std::optional<Scissor> scissor;
	};

}
