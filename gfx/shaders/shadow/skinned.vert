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

layout(std430, set = 2, binding = 7) readonly buffer BonePalette
{
    mat4 palette[];
};


void main()
{
    InstanceData instance = instances[gl_InstanceIndex];

    uint boneBase = instance.boneOffset;

    mat4 skin =
        aBoneWeights.x * palette[boneBase + uint(aBoneIndices.x)] +
        aBoneWeights.y * palette[boneBase + uint(aBoneIndices.y)] +
        aBoneWeights.z * palette[boneBase + uint(aBoneIndices.z)] +
        aBoneWeights.w * palette[boneBase + uint(aBoneIndices.w)];

    vec4 localPos = skin * vec4(aPos, 1.0);
    vec4 world    = instance.model * localPos;

    gl_Position = frame.shadow.viewProj * world;
}



