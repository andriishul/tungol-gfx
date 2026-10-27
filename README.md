# 🌟 tungol-gfx

A lightweight 3D rendering library built on Vulkan.

**Graphics API:** Vulkan 1.3+
**Dependencies:** GLFW, STB
**Supports:** one PCF shadow map per scene, compute skinning, simple single-light PBR

## Gallery

**1. Big Scene**
<video src="clips/BigScene.mp4" controls="controls" muted="muted" style="max-width: 100%;"></video>

**2. Small Scene**
<video src="clips/SingleScene.mp4" controls="controls" muted="muted" style="max-width: 100%;"></video>

**3. 4 Viewports, same Scene**
![4 Viewports](clips/4-viewports-same-scene.gif)

Code examples are in the `examples/` folder.

## Architecture

See [Architecture.md](docs/Architecture.md) for the design deep-dive.

## Build

Requires CMake and a C++ 23 compiler.