#version 450

void main()
{
}

/*void main()
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
    vCameraPosition = frame.camera.position;
}*/
