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

#include <chrono>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdint>


static float s_LastX = 0.0f;
static float s_LastY = 0.0f;
static bool s_FirstMouse = true;
static bool s_TabPressed = false;

static tungol::core::Camera s_Camera;

struct StreamButton
{
    tungol::Key key;
    bool wasDown = false;
    std::vector<uint32_t> spawned;
};

static StreamButton s_buttons[9];
static tungol::core::ModelID s_gargoyleId = tungol::core::EMPTY_MODEL;
static bool s_streamReady = false;


static std::vector<uint32_t> s_robotsSpawned;
static bool s_qWasDown = false;
static bool s_eWasDown = false;
static tungol::core::ModelID s_robotId = tungol::core::EMPTY_MODEL;
static std::string s_robotClip;
static bool s_robotStreamReady = false;


struct OrcAnimButton
{
    tungol::Key key;
    std::string clipName;
    bool wasDown = false;
};

static uint32_t s_orcIdx = UINT32_MAX;
static std::vector<OrcAnimButton> s_orcButtons;
static bool s_orcStreamReady = false;


static auto TRS(tungol::math::vec3 pos, tungol::math::vec3 rotDeg, tungol::math::vec3 scale)
{
    return tungol::math::translate(pos) *
        tungol::math::rotate(tungol::math::vec3(1, 0, 0), tungol::math::to_radians(rotDeg.x)) *
        tungol::math::rotate(tungol::math::vec3(0, 1, 0), tungol::math::to_radians(rotDeg.y)) *
        tungol::math::rotate(tungol::math::vec3(0, 0, 1), tungol::math::to_radians(rotDeg.z)) *
        tungol::math::scale(scale);
};


static void setupGargoyleStreaming(tungol::core::ModelID gargoyle)
{
    s_gargoyleId = gargoyle;

    s_buttons[0].key = tungol::Key::Num1;
    s_buttons[1].key = tungol::Key::Num2;
    s_buttons[2].key = tungol::Key::Num3;
    s_buttons[3].key = tungol::Key::Num4;
    s_buttons[4].key = tungol::Key::Num5;
    s_buttons[5].key = tungol::Key::Num6;
    s_buttons[6].key = tungol::Key::Num7;
    s_buttons[7].key = tungol::Key::Num8;
    s_buttons[8].key = tungol::Key::Num9;

    s_streamReady = true;
}

// Streamed gargoyle - buttons from 1 to 9 spawns/despawns 100 small statues
static void handleGargoyleStreaming(tungol::core::Scene& scene, const tungol::Input& input)
{
    if (!s_streamReady) return;

    for (int slot = 0; slot < 9; ++slot)
    {
        StreamButton& b = s_buttons[slot];

        const bool isDown = input.pressed(b.key);
        const bool rising = isDown && !b.wasDown;
        const bool falling = !isDown && b.wasDown;

        if (rising && b.spawned.empty())
        {
            constexpr int cols = 10;
            constexpr float spacing = 120.0f;
            constexpr float baseY = 30.0f;

            const float baseX = -3000.0f + slot * 800.0f;
            const float baseZ = -2500.0f;

            b.spawned.reserve(100);

            for (int i = 0; i < 100; ++i)
            {
                const int col = i % cols;
                const int row = i / cols;

                const float x = baseX + col * spacing;
                const float z = baseZ - row * spacing;

                tungol::core::Instance inst;
                inst.modelID = s_gargoyleId;
                inst.worldTransform = TRS({ x, baseY, z }, { -90, 0, 0 }, { 100, 100, 100 });
                inst.active = true;
                inst.clipName = "";
                inst.time = 0.0f;

                b.spawned.push_back(scene.spawn(inst));
            }
        }

        if (falling && !b.spawned.empty())
        {
            for (uint32_t idx : b.spawned)
                scene.despawn(idx);

            b.spawned.clear();
        }

        b.wasDown = isDown;
    }
}

static void setupRobotStreaming(tungol::core::ModelID robot, const std::string& clip)
{
    s_robotId = robot;
    s_robotClip = clip;
    s_robotStreamReady = true;
}

// Streamed robots — Q spawns/despawns 10 robots in a row, E advances animation
static void handleRobotStreaming(tungol::core::Scene& scene, const tungol::Input& input, float dt)
{
    if (!s_robotStreamReady) return;

    const bool qDown = input.pressed(tungol::Key::Q);
    const bool eDown = input.pressed(tungol::Key::E);

    const bool qRising = qDown && !s_qWasDown;
    const bool qFalling = !qDown && s_qWasDown;

    if (qRising && s_robotsSpawned.empty())
    {
        constexpr int count = 10;
        constexpr float spacing = 80.0f;
        constexpr float baseX = 100.0f;
        constexpr float baseZ = 400.0f;
        constexpr float baseY = 0.0f;

        s_robotsSpawned.reserve(count);

        for (int i = 0; i < count; ++i)
        {
            tungol::core::Instance inst;
            inst.modelID = s_robotId;
            inst.worldTransform = TRS({ baseX + i * spacing, baseY, baseZ }, { 0, 0, 0 }, { 100, 100, 100 });
            inst.active = true;
            inst.clipName = s_robotClip;
            inst.time = 0.0f;

            s_robotsSpawned.push_back(scene.spawn(inst));
        }
    }

    if (qFalling && !s_robotsSpawned.empty())
    {
        for (uint32_t idx : s_robotsSpawned)
            scene.despawn(idx);

        s_robotsSpawned.clear();
    }

    if (eDown && !s_robotsSpawned.empty())
    {
        for (uint32_t idx : s_robotsSpawned)
        {
            if (idx < scene.instances().size())
                scene.instance(idx).time() += dt;
        }
    }

    s_qWasDown = qDown;
    s_eWasDown = eDown;
}

// Moves the artillery forward/backward with Up/Down arrow keys.
static void handleArtilleryMovement(tungol::core::Scene& scene, uint32_t artilleryIdx, tungol::math::vec3& position,
    const tungol::math::vec3& rotationDeg, const tungol::math::vec3& scale, const tungol::Input& input, float dt)
{
    if (artilleryIdx >= scene.instances().size()) return;

    const float speed = 300.0f * std::min(dt, 0.1f);
    const float yawRad = tungol::math::to_radians(rotationDeg.y);
    const tungol::math::vec3 forward(std::sin(yawRad), 0.0f, std::cos(yawRad));

    bool moved = false;
    if (input.pressed(tungol::Key::Up))
    {
        position = position - (forward * speed);
        moved = true;
    }
    if (input.pressed(tungol::Key::Down))
    {
        position = position + (forward * speed);
        moved = true;
    }

    if (moved)
    {
        scene.instance(artilleryIdx).worldTransform() = TRS(position, rotationDeg, scale);
    }
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

    if (input.pressed(tungol::Key::W)) s_Camera.position = s_Camera.position + (forward * speed);
    if (input.pressed(tungol::Key::S)) s_Camera.position = s_Camera.position - (forward * speed);
    if (input.pressed(tungol::Key::A)) s_Camera.position = s_Camera.position - (right * speed);
    if (input.pressed(tungol::Key::D)) s_Camera.position = s_Camera.position + (right * speed);
    if (input.pressed(tungol::Key::Space)) s_Camera.position = s_Camera.position + (up * speed);
    if (input.pressed(tungol::Key::LeftControl)) s_Camera.position = s_Camera.position - (up * speed);

    if (input.pressed(tungol::Key::Tab)) s_TabPressed = true;
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
        tungol::event::subscribe_graphics_default_logger();
        tungol::event::EventBus::instance().sub(tungol::event::EventType::MouseMoved, onMouseMoved);
        tungol::GlfwContext::init();

        tungol::GlfwWindow window("tungol-gfx | Loading...", 1280, 720);
        tungol::GlfwContext::setEvents(window);
        tungol::GlfwInput input(window);

        tungol::core::AssetCache assets;

        tungol::runtime::RenderKernel kernel(window, assets);
        tungol::runtime::Renderer renderer(kernel);

        // ================== Models ==================
        auto floorModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("stone_floor/stone_floor.model"));
        auto wallModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("stone_wall/stone_wall.model"));
        auto lampModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("lamp/lamp.model"));

        auto robotModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("robot/robot.model"));
        auto robotPoliceModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("robot_police/robot_police.model"));
        auto steamModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("steampunk_robot/steampunk_robot.model"));
        auto artilleryModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("artillery_mech/artillery_mech.model"));

        auto gargoyleModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("gargoyle_statue/gargoyle_statue.model"));
        auto angelModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("graveyard_angel_statue/graveyard_angel_statue.model"));
        auto japaneseHouseModelId = assets.loadAsync(tungol::core::PathResolver::GetAssetPath("japanese_house/japanese_house.model"));
        //// ================== Scene setup ==================
        tungol::core::Scene scene;

        scene.addInstance(floorModelId, TRS({ 0, 0, 0 }, { 0,   0,  0 }, { 100.f, 100.f, 100.f }));
        scene.addInstance(wallModelId, TRS({ -500, 0, -1900 }, { 0, -90,  0 }, { 100.f, 100.f, 100.f }));

        auto robotPoliceModelIdx = scene.addInstance(robotPoliceModelId, TRS({ -150, 0, -500 }, { 0, 0, 0 }, { 100, 100, 100 }));
        auto steamIdx = scene.addInstance(steamModelId, TRS({ 150, 0, 0 }, { 0, 0, 0 }, { 100, 100, 100 }));
        tungol::math::vec3 artilleryPosition{ 500, 0, -1300 };
        const tungol::math::vec3 artilleryRotation{ 0, 180, 0 };
        const tungol::math::vec3 artilleryScale{ 50, 50, 50 };
        auto artilleryIdx = scene.addInstance(artilleryModelId, TRS(artilleryPosition, artilleryRotation, artilleryScale));

        const tungol::math::vec3 lampPosition{ -500, 10, 200 };
        scene.addInstance(lampModelId, TRS(lampPosition, { -90.f, 0, 75.f }, { 300, 300, 300 }));

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

                scene.addInstance(angelModelId, TRS(
                    { baseX + randomX, 0.0f, baseZ + randomZ },
                    { -90.0f, randomRotY, 0.0f },
                    { randomScale, randomScale, randomScale }));
            }
        }

        scene.addInstance(japaneseHouseModelId, TRS({ -2500, 0, -2000 }, { 0, 180 ,0 }, { 100, 100, 100 }));

        scene.addInstance(gargoyleModelId, TRS({ -1500.f, 30.f, -600.f }, { -90, 0, 0 }, { 100, 100, 100 }));
        scene.addInstance(gargoyleModelId, TRS({ -1500.f, 30.f, -200.f }, { -90, 0, 0 }, { 100, 100, 100 }));

        setupGargoyleStreaming(gargoyleModelId);
        setupRobotStreaming(robotModelId, "Armature|ArmatureAction");

        //// ================== Camera + light ==================
        const tungol::math::vec3 cameraPosition{ 400.0f, 150.0f, 500.0f };
        s_Camera.projection = tungol::math::perspective(tungol::math::to_radians(45.0f), 16.f / 9.f, 0.1f, 10000.0f);
        s_Camera.view = tungol::math::lookAt(cameraPosition, { 0.0f, 100.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        s_Camera.position = cameraPosition;

        const tungol::math::vec3 lightTarget{ 0.0f, -600.0f, -1500.0f };
        const tungol::math::vec3 lightDir = tungol::math::normalize(lightTarget - lampPosition);

        tungol::core::Light ld
        {
            .direction = lightDir,
            .intensity = 2.5f,
            .color = {  0.75f, 0.85f, 1.0f },
        };

        tungol::core::ViewData vd1{ s_Camera, ld };
        vd1.clearColor = { 0.01f, 0.02f, 0.04f, 1.0f };

        //// ================== Animation clip assignment ==================
        scene.instance(robotPoliceModelIdx).clipName() = "3903460652032_TempMotion";
        scene.instance(steamIdx).clipName() = "Armature.001|ArmatureAction.001";
        scene.instance(artilleryIdx).clipName() = "Pelter|Action";

        //// ================== Frame loop ==================
        auto lastTime = std::chrono::steady_clock::now();
        float fpsTimer = 0.0f;
        int frameCount = 0;
        float currentFps = 0.0f;
        float currentMs = 0.0f;

        float s_statsTimer = 0.0f;

        while (window.running())
        {
            window.pollEvents();
            assets.poll();

            auto  now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            fpsTimer += dt;
            ++frameCount;
            if (fpsTimer >= 1.0f)
            {
                currentFps = frameCount / fpsTimer;
                currentMs = (fpsTimer / frameCount) * 1000.0f;
                fpsTimer = 0.0f;
                frameCount = 0;
            }

            s_statsTimer += dt;
            if (s_statsTimer >= 2.0f)
            {
                s_statsTimer = 0.0f;

                size_t gargoyles = 0;
                for (const auto& b : s_buttons)
                    gargoyles += b.spawned.size();
            }


            if (robotPoliceModelIdx < scene.instances().size())
                scene.instance(robotPoliceModelIdx).time() += dt;

            if (steamIdx < scene.instances().size())
                scene.instance(steamIdx).time() += dt;

            if (artilleryIdx < scene.instances().size())
                scene.instance(artilleryIdx).time() += dt;


            handleInput(dt, input);

            handleArtilleryMovement(scene, artilleryIdx, artilleryPosition, artilleryRotation, artilleryScale, input, dt);

            handleRobotStreaming(scene, input, dt);

            handleGargoyleStreaming(scene, input);

            vd1.camera = s_Camera;

            renderer.render(vd1, scene);

            char buf[160];
            std::snprintf(buf, sizeof(buf), "tungol-gfx | %.0f FPS | %.2f ms", currentFps, currentMs);
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