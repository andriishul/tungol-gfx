cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "tungol-gfx requires 64-bit.")
endif()

if(CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    message(FATAL_ERROR "tungol-gfx requires x64. Detected: ${CMAKE_GENERATOR_PLATFORM}.")
endif()

project(TUNGOL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
add_subdirectory(gfx/src/thirdparty/glfw)

set(GLFW_WRAPPERS_SRCS
    gfx/src/window/glfw/GlfwContext.cpp
    gfx/src/window/glfw/GlfwWindow.cpp
    gfx/src/window/glfw/GlfwInput.cpp
    gfx/src/window/surface/SurfaceProvider.cpp
)

set(CORE_SRCS
    gfx/src/core/PathResolver.cpp
)

set(RUNTIME_SRCS
    gfx/src/runtime/AnimationResources.cpp
    gfx/src/runtime/ShadowResources.cpp
    gfx/src/runtime/RenderKernel.cpp
    gfx/src/runtime/RenderScene.cpp
    gfx/src/runtime/Renderer.cpp
    gfx/src/runtime/Texture.cpp
)

set(BUFFERS_SRCS
    gfx/src/vhl/core/buffers/Buffer.cpp
    gfx/src/vhl/core/buffers/StagingBuffer.cpp
    gfx/src/vhl/core/buffers/VertexBuffer.cpp
    gfx/src/vhl/core/buffers/IndexBuffer.cpp
    gfx/src/vhl/core/buffers/UniformBuffer.cpp
    gfx/src/vhl/core/buffers/IndirectBuffer.cpp
    gfx/src/vhl/core/buffers/StorageBuffer.cpp
)

set(IMAGES_SRCS
    gfx/src/vhl/core/images/Image.cpp
    gfx/src/vhl/core/images/ImageView.cpp
    gfx/src/vhl/core/images/Sampler.cpp
)

set(GRAPH_SRCS
    gfx/src/vhl/graph/CommandEncoder.cpp
    gfx/src/vhl/graph/HazardTracker.cpp
    gfx/src/vhl/graph/CommandQueue.cpp
)

set(VHL_SRCS
    gfx/src/vhl/core/GraphicsContext.cpp
    gfx/src/vhl/core/Device.cpp
    gfx/src/vhl/core/Queue.cpp
    gfx/src/vhl/core/FormatSupport.cpp
    gfx/src/vhl/core/Swapchain.cpp
    gfx/src/vhl/core/Descriptors.cpp
    gfx/src/vhl/core/Allocator.cpp

    gfx/src/vhl/pipeline/ComputePipeline.cpp
    gfx/src/vhl/pipeline/GraphicsPipeline.cpp

    gfx/src/vhl/commands/CommandBuffer.cpp
    gfx/src/vhl/commands/BarrierCommand.cpp
    gfx/src/vhl/commands/CommandRecorder.cpp
    gfx/src/vhl/commands/CopyContext.cpp

    gfx/src/vhl/sync/Fence.cpp
    gfx/src/vhl/sync/Semaphore.cpp

    gfx/src/vhl/utils/VkInspector.cpp
)

set(ENGINE_SOURCES
    ${GLFW_WRAPPERS_SRCS}
    ${CORE_SRCS}
    ${RUNTIME_SRCS}
    ${BUFFERS_SRCS}
    ${IMAGES_SRCS}
    ${GRAPH_SRCS}
    ${VHL_SRCS}
)

add_library(tungol-gfx SHARED ${ENGINE_SOURCES})

target_include_directories(tungol-gfx
    PUBLIC  gfx/src
            gfx/src/math
    PRIVATE ${STB_INCLUDE_DIRECTORIES}
)

target_link_libraries(tungol-gfx PRIVATE glfw Vulkan::Vulkan)

target_compile_definitions(tungol-gfx
    PRIVATE TUNGOL_EXPORTS
    PRIVATE TUNGOL_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/gfx/shaders"
    PRIVATE TUNGOL_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/gfx/assets"
)

if(MSVC)
    target_compile_options(tungol-gfx PRIVATE /wd4251)
endif()

add_executable(app app/src/main.cpp)
target_link_libraries(app PRIVATE tungol-gfx)