#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;

#include "vertex_common.glsl"

void main()
{
    Instance instance = instances[gl_InstanceIndex];

    gl_Position = viewData.shadow.viewProj * instance.worldTransform * vec4(aPos, 1.0);
    
}