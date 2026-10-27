#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define SHADOW_SET 3

#include "shade_fragment.glsl"

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;
layout(location = 4) in vec3 vTangent;
layout(location = 5) in vec3 vBitangent;
layout(location = 6) in vec3 vCameraPosition;
layout(location = 7) in LightData vLight;
layout(location = 10) in ShadowData vShadow;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = shade_fragment(vNormal, vUV, vWorldPos, 
                              vTangent, vBitangent, vMaterialIndex,
                              vLight, vShadow, vCameraPosition);
}
