#include <iostream>

#include <window/glfw/GlfwWindow.hpp>
#include <window/glfw/GlfwContext.hpp>
#include <window/glfw/GlfwInput.hpp>

#include <event/EventBus.hpp>
#include <event/MouseMovedEventArgs.hpp>

#include <core/PathResolver.hpp>
#include <core/ModelRegistry.hpp>

#include <runtime/RenderScene.hpp>
#include <runtime/Renderer.hpp>

#include <math/transform.hpp>
#include <math/vm.hpp>

#include <chrono>

static float s_LastX = 0.0f;
static float s_LastY = 0.0f;
static bool s_FirstMouse = true;
static bool s_TabPressed = false;

static tungol::core::Camera s_Camera;


auto TRS(tungol::math::vec3 pos, tungol::math::vec3 rotDeg, tungol::math::vec3 scale)
{
    return tungol::math::translate(pos) *
        tungol::math::rotate(tungol::math::vec3(1, 0, 0), tungol::math::to_radians(rotDeg.x)) *
        tungol::math::rotate(tungol::math::vec3(0, 1, 0), tungol::math::to_radians(rotDeg.y)) *
        tungol::math::rotate(tungol::math::vec3(0, 0, 1), tungol::math::to_radians(rotDeg.z)) *
        tungol::math::scale(scale);
};

// Camera movement (WASD for horizontal, Space/Ctrl for vertical)
void handleInput(float dt, const tungol::Input& input)
{
    const float speed = 500.f * std::min(dt, 0.1f);

    const tungol::math::vec3 right(s_Camera.view[0][0], s_Camera.view[1][0], s_Camera.view[2][0]);
    const tungol::math::vec3 forward(-s_Camera.view[0][2], -s_Camera.view[1][2], -s_Camera.view[2][2]);
    const tungol::math::vec3 up(0.0f, 1.0f, 0.0f);

    if (input.pressed(tungol::Key::W)) s_Camera.position = s_Camera.position + (forward * speed);
    if (input.pressed(tungol::Key::S)) s_Camera.position = s_Camera.position - (forward * speed);
    if (input.pressed(tungol::Key::A)) s_Camera.position = s_Camera.position - (right * speed);
    if (input.pressed(tungol::Key::D)) s_Camera.position = s_Camera.position + (right * speed);
    if (input.pressed(tungol::Key::Space)) s_Camera.position = s_Camera.position + (up * speed);
    if (input.pressed(tungol::Key::LeftControl)) s_Camera.position = s_Camera.position - (up * speed);

    if (input.pressed(tungol::Key::Tab))
        s_TabPressed = true;
    if (input.released(tungol::Key::Tab))
        s_TabPressed = false;

    s_Camera.view = tungol::math::lookAt(s_Camera.position, s_Camera.position + forward, up);
}

void onMouseMoved(const tungol::event::EventArgs& args)
{
    if (args.type() == tungol::event::EventType::MouseMoved)
    {
        const auto& mouseMovedArgs = (const tungol::event::MouseMovedEventArgs&)args;

        float x = mouseMovedArgs.xPos();
        float y = mouseMovedArgs.yPos();

        if (s_TabPressed)
        {
            if (s_FirstMouse)
            {
                s_LastX = x;
                s_LastY = y;
                s_FirstMouse = false;
            }

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
        else
        {
            s_FirstMouse = true;
        }
    }
}


int main()
{
    try
    {
        // 1. GLFW context, window, and logging
        tungol::event::subscribe_graphics_default_logger();
        tungol::event::EventBus::instance().sub(tungol::event::EventType::MouseMoved, onMouseMoved);
        tungol::GlfwContext::init();

        tungol::GlfwWindow window("tungol-gfx | Loading...", 1280, 720);

        tungol::GlfwContext::setEvents(window);

        tungol::GlfwInput input(window);

        // 2. Model loader
        tungol::asset::ModelReader reader;

        // 3. Kernel and renderer setup
        tungol::runtime::RenderKernel kernel(window);
        tungol::runtime::Renderer renderer(kernel);

        // 4. Load all assets
        tungol::core::ModelRegistry models;

        auto floorModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("stone_floor/stone_floor.model")));
        auto wallModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("stone_wall/stone_wall.model")));
        auto lampModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("lamp/lamp.model")));

        auto robotModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("robot/robot.model")));
        auto steamModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("steampunk_robot/steampunk_robot.model")));
        auto policeModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("robot_police/robot_police.model")));
        auto artilleryModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("artillery_mech/artillery_mech.model")));

        auto gargoyleModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("gargoyle_statue/gargoyle_statue.model")));
        auto angelModelId = models.add(reader.read(tungol::core::PathResolver::GetAssetPath("graveyard_angel_statue/graveyard_angel_statue.model")));

        // 5. Build the Scene
        tungol::core::SceneBuilder builder(models);

        builder.addInstance(floorModelId, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 100.f, 100.f, 100.f }));
        builder.addInstance(wallModelId, TRS({ -500, 0, -1900 }, { 0, -90, 0 }, { 100.f, 100.f, 100.f }));

        uint32_t robotIdx = builder.addInstance(robotModelId, TRS({ -150, 0, 0 }, { 0, 0, 0 }, { 100, 100, 100 }));
        uint32_t steamIdx = builder.addInstance(steamModelId, TRS({ 150, 0, 0 }, { 0, 0, 0 }, { 100, 100, 100 }));


        tungol::math::vec3 artilleryPosition{ 500, 0, -1300 };
        uint32_t artilleryIdx = builder.addInstance(artilleryModelId, TRS(artilleryPosition, { 0, 180, 0 }, { 50, 50, 50 }));

        uint32_t policeIdx = builder.addInstance(policeModelId, TRS({ -500, 0, -200 }, { 0, 45, 0 }, { 100, 100, 100 }));


        const tungol::math::vec3 lampPosition{ -500, 10, 200 };
        builder.addInstance(lampModelId, TRS(lampPosition, { -90.f, 0, 75.f }, { 300, 300, 300 }));

        const float statueScale = 25.0f;

        for (int x = -10; x < 10; ++x)
        {
            for (int z = 0; z < 5; ++z)
            {
                float baseX = x * 150.0f;
                float baseZ = -1500.0f - (z * 150.0f);

                float randomX = static_cast<float>(std::rand() % 81) - 40.0f;
                float randomZ = static_cast<float>(std::rand() % 81) - 40.0f;

                float randomRotY = static_cast<float>(std::rand() % 51) - 25.0f;

                float randomScale = statueScale + (static_cast<float>(std::rand() % 11) - 5.0f);

                builder.addInstance(angelModelId, TRS(
                    { baseX + randomX, 0.0f, baseZ + randomZ },
                    { -90.0f, randomRotY, 0.0f },
                    { randomScale, randomScale, randomScale }
                ));
            }
        }
        builder.addInstance(gargoyleModelId, TRS({ -1500.f, 30.f, -600.f }, { -90, 0, 0 }, { 100, 100, 100 }));
        builder.addInstance(gargoyleModelId, TRS({ -1500.f, 30.f, -200 }, { -90, 0, 0 }, { 100, 100, 100 }));

        // Build and upload the scene
        tungol::core::Scene scene = builder.build();

        tungol::runtime::RenderScene rScene(kernel, models, scene);

        auto& robot = rScene.scene().instance(robotIdx);
        auto& steampunk = rScene.scene().instance(steamIdx);
        auto& artillery = rScene.scene().instance(artilleryIdx);
        auto& police = rScene.scene().instance(policeIdx);

        robot.clipName = "Armature|ArmatureAction";
        steampunk.clipName = "Armature.001|ArmatureAction.001";
        artillery.clipName = "Pelter|Action";
        police.clipName = "3903460652032_TempMotion";

        const tungol::math::vec3 cameraPosition{ 400.0f, 150.0f, 500.0f };
        s_Camera.projection = tungol::math::perspective(tungol::math::to_radians(45.0f), 16.f / 9.f, 0.1f, 10000.0f);
        s_Camera.view = tungol::math::lookAt(cameraPosition, tungol::math::vec3{ 0.0f, 100.0f, 0.0f }, tungol::math::vec3{ 0.0f, 1.0f, 0.0f });
        s_Camera.position = cameraPosition;


        const tungol::math::vec3 lightTarget{ 0.0f, -300.f, 0.0f };
        const tungol::math::vec3 lightDir = tungol::math::normalize(lightTarget - lampPosition);

        tungol::core::Light ld
        {
            .direction = lightDir,
            .intensity = 2.5f,
            .color = { 1.0f, 0.95f, 0.9f },
        };

        tungol::core::ViewData vd1{ s_Camera, ld };
        vd1.clearColor = { 0.1f, 0.1f, 0.15f, 1.0f };

        // 9. Main loop
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


            artillery.transform = TRS(artilleryPosition, { 0, 180, 0 }, { 50, 50, 50 });

            // Update Animations
            robot.time += dt;
            steampunk.time += dt;
            artillery.time += dt;
            police.time += dt;

            handleInput(dt, input);

            vd1.camera = s_Camera;

            renderer.begin();

            renderer.draw(rScene, vd1);

            renderer.end();

            artilleryPosition.z += 50 * dt;

            char buf[128];
            std::snprintf(buf, sizeof(buf), "tungol-gfx | Big Scene | %.0f FPS | %.2f ms", currentFps, currentMs);
            window.setTitle(buf);
        }

    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return tungol::GlfwContext::close();
    }

    return tungol::GlfwContext::close();
}