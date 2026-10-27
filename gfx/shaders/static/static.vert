#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;

#include "vertex_common.glsl"

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) flat out uint vMaterialIndex;
layout(location = 4) out vec3 vTangent;
layout(location = 5) out vec3 vBitangent;
layout(location = 6) out vec3 vCameraPosition;
layout(location = 7) out LightData vLight;
layout(location = 10) out ShadowData vShadow;

void main()
{
    InstanceData instance = instances[gl_InstanceIndex];

    gl_Position = frame.camera.projection * frame.camera.view * instance.model * vec4(aPos, 1.0);
    
    vec4 world = instance.model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vUV = aUV;

    vec3 N = mat3(instance.model) * aNormal;
    vec3 T = mat3(instance.model) * aTangent.xyz;
    vec3 B = cross(N, T) * aTangent.w;

    vNormal = N;
    vTangent = T;
    vBitangent = B;

    vMaterialIndex = instance.materialIndex;
    
    vLight = frame.light;
    vShadow = frame.shadow;
    vCameraPosition = frame.camera.position;
}
