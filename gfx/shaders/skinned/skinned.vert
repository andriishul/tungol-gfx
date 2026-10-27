#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in ivec4 aBoneIndices;
layout(location = 5) in vec4 aBoneWeights;

#include "vertex_common.glsl"
#include "bone_common.glsl"

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) flat out uint vMaterialIndex;
layout(location = 4) out vec3 vTangent;
layout(location = 5) out vec3 vBitangent;
layout(location = 6) out vec3 vCameraPosition;
layout(location = 7) out LightData vLight;
layout(location = 10) out ShadowData vShadow;


layout(std430, set = 3, binding = 7) readonly buffer BonePalette
{
    mat4 palette[];
};


void main()
{
    InstanceData instance = instances[gl_InstanceIndex];

    uint boneBase = instance.boneOffset;
   
    mat4 skin = aBoneWeights.x * palette[boneBase + uint(aBoneIndices.x)]
        + aBoneWeights.y * palette[boneBase + uint(aBoneIndices.y)]
        + aBoneWeights.z * palette[boneBase + uint(aBoneIndices.z)]
        + aBoneWeights.w * palette[boneBase + uint(aBoneIndices.w)];

    vec4 localPos = skin * vec4(aPos, 1.0);
    vec3 localNrm = mat3(skin) * aNormal;
    vec3 localTan = mat3(skin) * aTangent.xyz;

    vec4 world = instance.model * localPos;

    gl_Position = frame.camera.projection * frame.camera.view * world;

    vWorldPos = world.xyz;
    vUV = aUV;

    vec3 N = mat3(instance.model) * localNrm;
    vec3 T = mat3(instance.model) * localTan;
    vec3 B = cross(N, T) * aTangent.w;

    vNormal = N;
    vTangent = T;
    vBitangent = B;
    vMaterialIndex = instance.materialIndex;
    
    vLight = frame.light;
    vShadow = frame.shadow;
    vCameraPosition = frame.camera.position;
}



