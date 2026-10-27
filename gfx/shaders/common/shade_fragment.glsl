#ifndef TUNGOL_COMMON_SHADE_FRAGMENT_GLSL
#define TUNGOL_COMMON_SHADE_FRAGMENT_GLSL

#include "fragment_common.glsl"


float shadowFactor(ShadowData shadow, vec3 worldPos, vec3 N, vec3 L)
{
    vec4 lp = shadow.viewProj * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    float ndl  = max(dot(N, L), 0.0);
    float bias = max(shadow.bias * (1.0 - ndl), 0.0001);
    float depth = proj.z - bias;

    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));

    float sum = 0.0;
    
    // 3x3 PCF with hardware comparison
    for (int x = -1; x <= 1; ++x) 
    {
        for (int y = -1; y <= 1; ++y) 
        {
            sum += texture(shadowMap, vec3(proj.xy + vec2(x, y) * texel, depth));
        }
    }

    return sum / 9.0;
}


vec4 shade_fragment(vec3 N_geom, vec2 uv, vec3 worldPos, vec3 T, vec3 B, uint matIdx, LightData light, ShadowData shadow, vec3 camPos)
{
    MaterialData mat = materials[matIdx == UINT32_MAX ? 0 : matIdx];
    vec4 albedo = albedo_texture(mat, uv) * mat.baseColor;

    mat3 TBN = mat3(normalize(T), normalize(B), normalize(N_geom));
    vec3 nrmSample = normal_texture(mat, uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(TBN * nrmSample);

    vec3 V = normalize(camPos - worldPos);
    vec3 L = normalize(-light.direction);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec4 mrSample = mr_texture(mat, uv);
    float metallic = clamp(mrSample.b * mat.metallic, 0.0, 1.0);
    float roughness = clamp(mrSample.g * mat.roughness, 0.04, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-6;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    float lit = shadowFactor(shadow, worldPos, N, L);

    vec3 radiance = light.color * light.intensity;
    vec3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL * lit;

    vec3 ambient = albedo.rgb * 0.05;
    vec3 emissive = emissive_texture(mat, uv).rgb;

    vec3 color = ambient + Lo + emissive;
    color = acesTonemap(color);

    return vec4(color, albedo.a);
}

#endif
