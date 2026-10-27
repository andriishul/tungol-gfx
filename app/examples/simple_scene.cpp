#include <iostream>

#include <window/glfw/GlfwWindow.hpp>
#include <window/glfw/GlfwContext.hpp>

#include <event/EventBus.hpp>

#include <core/PathResolver.hpp>
#include <core/ModelRegistry.hpp>

#include <runtime/RenderScene.hpp>
#include <runtime/Renderer.hpp>

#include <math/transform.hpp>
#include <math/vm.hpp>

#include <chrono>

int main()
{
	try
	{
		// 1. GLFW context, window, and logging
		tungol::event::subscribe_graphics_default_logger();
		tungol::GlfwContext::init();
		tungol::GlfwWindow window("tungol-gfx | Loading", 800, 600);

		// 2. Model loader
		tungol::asset::ModelReader reader;

		// 3. Kernel and renderer setup
		tungol::runtime::RenderKernel kernel(window);
		tungol::runtime::Renderer renderer(kernel);

		// 4. Scene creation and upload
		tungol::core::ModelRegistry models;

		auto firstModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("robot/robot.model")));
		auto secondModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("steampunk_robot/steampunk_robot.model")));
		auto thirdModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("wooden_floor/wooden_floor.model")));

		tungol::math::vec3 robotPosition(-150.0f, 0.0f, 0.0f);
		tungol::math::vec3 steampunkRobotPosition(150.0f, 0.0f, 0.0f);

		tungol::core::SceneBuilder builder(models);
		uint32_t robotIdx = builder.addInstance(firstModelId, tungol::math::translate(robotPosition) * tungol::math::scale(tungol::math::vec3(100.0f, 100.0f, 100.0f)));
		uint32_t steamIdx = builder.addInstance(secondModelId, tungol::math::translate(steampunkRobotPosition) * tungol::math::scale(tungol::math::vec3(100.0f, 100.0f, 100.0f)));
		uint32_t floorIdx = builder.addInstance(thirdModelId, tungol::math::rotate(tungol::math::vec3(1.0f, 0.0f, 0.0f), tungol::math::to_radians(90.0f)) * tungol::math::scale(tungol::math::vec3(20, 20, 20)));

		tungol::core::Scene scene = builder.build();

			
		tungol::runtime::RenderScene rScene1(kernel, models, scene);

		auto& robot = rScene1.scene().instance(robotIdx);
		auto& steampunkRobot = rScene1.scene().instance(steamIdx);

		robot.clipName = "Armature|ArmatureAction";
		steampunkRobot.clipName = "Armature.001|ArmatureAction.001";

		// 5. Camera and light setup
		tungol::math::vec3 cameraPosition{ 500.0f, 140.0f, 450.0f };

		tungol::core::Camera cd
		{
			.projection = tungol::math::perspective(tungol::math::to_radians(60.0f), 16.f / 9.f,
													0.1f, 10000.0f),
			.view = tungol::math::lookAt(cameraPosition,
				tungol::math::vec3{ 0.0f, 0.0f, 0.0f },
				tungol::math::vec3{ 0.0f, 1.0f, 0.0f }),

			.position = cameraPosition
		};

		tungol::core::Light ld
		{
			.direction = {-0.5f, -1.0f, -0.5f},
			.intensity = 1.0f,
			.color = {1, 1, 1},
		};

		tungol::core::ViewData fd1{ cd, ld };

		// 6. Main loop

		auto lastTime = std::chrono::steady_clock::now();
		float fpsTimer = 0.0f;
		int frameCount = 0;
		float currentFps = 0.0f;
		float currentMs = 0.0f;


		while (window.running())
		{
			window.pollEvents();

			auto now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(now - lastTime).count();
			lastTime = now;

			fpsTimer += dt;
			frameCount++;
			if (fpsTimer >= 1.0f)
			{
				currentFps = frameCount / fpsTimer;
				currentMs = (fpsTimer / frameCount) * 1000.0f;
				fpsTimer = 0.0f;
				frameCount = 0;
			}

			robot.time += dt;
			steampunkRobot.time += dt;

			robot.transform = tungol::math::translate(robotPosition) *
				tungol::math::scale(tungol::math::vec3(100, 100, 100));

			steampunkRobot.transform = tungol::math::translate(steampunkRobotPosition) *
				tungol::math::scale(tungol::math::vec3(100, 100, 100));

			renderer.begin();

			renderer.draw(rScene1, fd1);

			renderer.end();

			char buf[128];
			std::snprintf(buf, sizeof(buf), "tungol-gfx | %.0f FPS | %.2f ms", currentFps, currentMs);
			window.setTitle(buf);

			robotPosition.z += 0.05;
			steampunkRobotPosition.x += 0.05;
		}

	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return tungol::GlfwContext::close();
	}


	return tungol::GlfwContext::close();
}