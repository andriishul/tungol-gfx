#ifndef TUNGOL_COMMON_FRAMES_GLSL
#define TUNGOL_COMMON_FRAMES_GLSL

struct LightData
{
    vec3 direction;
    float intensity;
    vec3 color;
};

struct CameraData
{
    mat4 projection;
    mat4 view;
    vec3 position;
};

struct ShadowData
{
    mat4 viewProj;
    float bias;
};

struct FrameData
{
    CameraData camera;
    LightData light;
    ShadowData shadow;
};

layout(std140, set = 0, binding = 0) uniform FrameUBO { FrameData frame; };

#endif
