#include <iostream>

#include <window/glfw/GlfwWindow.hpp>
#include <window/glfw/GlfwContext.hpp>
#include <window/glfw/GlfwInput.hpp>

#include <event/EventBus.hpp>
#include <event/MouseMovedEventArgs.hpp>

#include <core/PathResolver.hpp>
#include <core/AssetCache.hpp>
#include <core/Scene.hpp>

#include <runtime/Renderer.hpp>

#include <math/transform.hpp>
#include <math/vm.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>

static float s_LastX = 0.0f;
static float s_LastY = 0.0f;
static bool s_FirstMouse = true;
static bool s_TabPressed = false;

static tungol::core::Camera s_Camera;

static auto TRS(tungol::math::vec3 pos, tungol::math::vec3 rotDeg, tungol::math::vec3 scale)
{
	return tungol::math::translate(pos) *
		tungol::math::rotate(tungol::math::vec3(1, 0, 0), tungol::math::to_radians(rotDeg.x)) *
		tungol::math::rotate(tungol::math::vec3(0, 1, 0), tungol::math::to_radians(rotDeg.y)) *
		tungol::math::rotate(tungol::math::vec3(0, 0, 1), tungol::math::to_radians(rotDeg.z)) *
		tungol::math::scale(scale);
}

/*
	W/S -> forward/backward,
	A/D -> left/right,
	Space/LCtrl -> up/down,
	Tab -> enable/disable mouse look
*/
static void handleInput(float dt, const tungol::Input& input)
{
	const float speed = 500.f * std::min(dt, 0.1f);

	const tungol::math::vec3 right(s_Camera.view[0][0], s_Camera.view[1][0], s_Camera.view[2][0]);
	const tungol::math::vec3 forward(-s_Camera.view[0][2], -s_Camera.view[1][2], -s_Camera.view[2][2]);
	const tungol::math::vec3 up(0.0f, 1.0f, 0.0f);

	if (input.pressed(tungol::Key::W))           s_Camera.position = s_Camera.position + (forward * speed);
	if (input.pressed(tungol::Key::S))           s_Camera.position = s_Camera.position - (forward * speed);
	if (input.pressed(tungol::Key::A))           s_Camera.position = s_Camera.position - (right * speed);
	if (input.pressed(tungol::Key::D))           s_Camera.position = s_Camera.position + (right * speed);
	if (input.pressed(tungol::Key::Space))       s_Camera.position = s_Camera.position + (up * speed);
	if (input.pressed(tungol::Key::LeftControl)) s_Camera.position = s_Camera.position - (up * speed);

	if (input.pressed(tungol::Key::Tab))  s_TabPressed = true;
	if (input.released(tungol::Key::Tab)) s_TabPressed = false;

	s_Camera.view = tungol::math::lookAt(s_Camera.position, s_Camera.position + forward, up);
}

/*
	Rotates camera view (yaw/pitch) based on mouse movement while Tab is held.
*/
static void onMouseMoved(const tungol::event::EventArgs& args)
{
	if (args.type() != tungol::event::EventType::MouseMoved) return;

	const auto& mouseMovedArgs = (const tungol::event::MouseMovedEventArgs&)args;

	float x = mouseMovedArgs.xPos();
	float y = mouseMovedArgs.yPos();

	if (!s_TabPressed) { s_FirstMouse = true; return; }

	if (s_FirstMouse) { s_LastX = x; s_LastY = y; s_FirstMouse = false; }

	float xOffset = x - s_LastX;
	float yOffset = s_LastY - y;
	s_LastX = x;
	s_LastY = y;

	const float sensitivity = 0.1f;
	float yawAngle = -xOffset * sensitivity;
	float pitchAngle = yOffset * sensitivity;

	tungol::math::vec3 right(s_Camera.view[0][0], s_Camera.view[1][0], s_Camera.view[2][0]);
	tungol::math::vec4 forward(-s_Camera.view[0][2], -s_Camera.view[1][2], -s_Camera.view[2][2], 1.0f);
	tungol::math::vec3 up(0.0f, 1.0f, 0.0f);

	auto yawMatrix = tungol::math::rotate(up, tungol::math::to_radians(yawAngle));
	auto pitchMatrix = tungol::math::rotate(right, tungol::math::to_radians(pitchAngle));

	forward = yawMatrix * forward;
	forward = pitchMatrix * forward;
	forward = tungol::math::normalize(forward);

	s_Camera.view = tungol::math::lookAt(s_Camera.position, s_Camera.position + forward, up);
}

int main()
{
	try
	{
		// 1. GLFW context, window, events, and logging
		tungol::event::subscribe_graphics_default_logger();
		tungol::event::EventBus::instance().sub(tungol::event::EventType::MouseMoved, onMouseMoved);
		tungol::GlfwContext::init();

		tungol::GlfwWindow window("tungol-gfx | Loading", 640, 360);
		tungol::GlfwContext::setEvents(window);
		tungol::GlfwInput input(window);

		// 2. Asset cache, kernel, and renderer setup
		tungol::core::AssetCache assets;
		tungol::runtime::RenderKernel kernel(window, assets);
		tungol::runtime::Renderer renderer(kernel);

		// 3. Load models asynchronously
		auto firstModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("robot/robot.model"));
		auto secondModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("steampunk_robot/steampunk_robot.model"));
		auto floorModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("wooden_floor/wooden_floor.model"));

		// 4. Scene setup
		tungol::math::vec3 robotPosition(-150.0f, 0.0f, 0.0f);
		tungol::math::vec3 steampunkRobotPosition(150.0f, 0.0f, 0.0f);

		tungol::core::Scene scene;
		uint32_t robotIdx = scene.addInstance(firstModelId, TRS(robotPosition, { 0, 0, 0 }, { 100.0f, 100.0f, 100.0f }));
		uint32_t steamIdx = scene.addInstance(secondModelId, TRS(steampunkRobotPosition, { 0, 0, 0 }, { 100.0f, 100.0f, 100.0f }));
		uint32_t floorIdx = scene.addInstance(floorModelId, TRS({ 0.0f, 0.0f, 0.0f }, { -90.0f, 0.0f, 0.0f }, { 100.0f, 100.0f, 100.0f }));

		auto robot = scene.instance(robotIdx);
		auto steampunkRobot = scene.instance(steamIdx);

		robot.clipName() = "Armature|ArmatureAction";
		steampunkRobot.clipName() = "Armature.001|ArmatureAction.001";

		// 5. Camera and light setup
		const tungol::math::vec3 cameraPosition{ 500.0f, 140.0f, 450.0f };
		s_Camera.projection = tungol::math::perspective(
			tungol::math::to_radians(60.0f), 16.f / 9.f, 0.1f, 10000.0f);
		s_Camera.view = tungol::math::lookAt(
			cameraPosition,
			tungol::math::vec3{ 0.0f, 0.0f, 0.0f },
			tungol::math::vec3{ 0.0f, 1.0f, 0.0f });
		s_Camera.position = cameraPosition;

		tungol::core::Light ld
		{
			.direction = {-0.5f, -1.0f, -0.5f},
			.intensity = 3.5f,
			.color = {1, 1, 1},
		};

		tungol::core::ViewData vd1{ s_Camera, ld };

		// 6. Main loop
		auto lastTime = std::chrono::steady_clock::now();
		float fpsTimer = 0.0f;
		int frameCount = 0;
		float currentFps = 0.0f;
		float currentMs = 0.0f;

		while (window.running())
		{
			window.pollEvents();
			assets.poll();

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

			handleInput(dt, input);

			robot.time() += dt;
			steampunkRobot.time() += dt;

			robot.worldTransform() = TRS(robotPosition, { 0, 0, 0 }, { 100.0f, 100.0f, 100.0f });
			steampunkRobot.worldTransform() = TRS(steampunkRobotPosition, { 0, 0, 0 }, { 100.0f, 100.0f, 100.0f });

			vd1.camera = s_Camera;


			renderer.render(vd1, scene);


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