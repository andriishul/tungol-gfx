#pragma once
#include <cstdint>

#include <math/types.hpp>

#include <string>
#include <vector>

namespace tungol::asset
{
    constexpr uint32_t MODEL_MAGIC = 0x4C444F4D;
    
    constexpr uint32_t MAX_STRING_LENGTH = 64U;

    /*
        1. AssetHeader
        2. MeshEntry
        3. AnimationChannel
        4. AnimationHeader

        5. StaticVertex
        6. SkinnedVertex
        7. MaterialEntry
        8. BoneEntry
        9. Frames BLOB
    */
    struct ModelHeader
    {
        uint32_t magic = MODEL_MAGIC;

        uint32_t meshCount = 0;
        uint32_t animationCount = 0;
        uint32_t materialCount = 0;
        uint32_t boneCount = 0;

        uint64_t meshTableOffset = 0;
        uint64_t animationTableOffset = 0;

        uint64_t materialTableOffset;
        uint64_t boneTableOffset = 0;

        uint64_t staticVertexBlobOffset = 0;
        uint64_t staticVertexBlobSize = 0;
        uint64_t skinnedVertexBlobOffset = 0;
        uint64_t skinnedVertexBlobSize = 0;

        uint64_t indexBlobOffset = 0;
        uint64_t indexBlobSize = 0;

        uint64_t channelTableOffset = 0;
        uint64_t channelTableSize = 0;

        uint64_t keyframeBlobOffset = 0;
        uint64_t keyframeBlobSize = 0;
    };


    struct StaticVertex
    {
        math::vec3 position;
        math::vec3 normal;
        math::vec2 uv;
        math::vec4 tangent;
    };

    struct SkinnedVertex
    {
        math::vec3 position;
        math::vec3 normal;
        math::vec2 uv;
        math::vec4 tangent;
        math::ivec4 boneIndices;
        math::vec4 boneWeights;
    };

    struct MaterialEntry
    {
        char albedoPath[MAX_STRING_LENGTH];
        char normalPath[MAX_STRING_LENGTH];
        char mrPath[MAX_STRING_LENGTH];
        char emissivePath[MAX_STRING_LENGTH];

        math::vec4 baseColor;
        float_t metallic = 0;
        float_t roughness = 0;
    };

    struct BoneEntry
    {
        char name[MAX_STRING_LENGTH];
        math::mat4 bindLocalTransform;
        math::mat4 inverseBindMatrix;
        int32_t parentIndex = -1;
    };

    struct MeshEntry
    {
        uint32_t vertexIndex = 0;    
        uint32_t vertexCount = 0;
        
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        
        uint32_t materialIndex = 0;
        uint32_t hasSkin = 0;
    };

    struct AnimationHeader
    {
        char name[64];
        float duration = 0.0f;
        uint32_t channelIndex = 0;
        uint32_t channelCount = 0;
    };

    struct AnimationKey
    {
        float time;
        math::vec3 position;
        math::quat rotation;
        math::vec3 scale;
    };

    struct AnimationChannel
    {
        uint32_t boneIndex = 0;
        uint32_t keyCount = 0;
        uint32_t keyIndex = 0;
    };

    
    struct Model
    {
        std::vector<MeshEntry> meshes;

        std::vector<StaticVertex> staticVertices;
        std::vector<SkinnedVertex> skinnedVertices;

        std::vector<uint32_t> indices;

        std::vector<MaterialEntry> materials;
        std::vector<BoneEntry> bones;

        std::vector<AnimationChannel> channels;
        std::vector<AnimationHeader> animations;
        std::vector<AnimationKey> frames;
    };

    struct ModelEntry
    {
        std::string path;
        Model model;
    };

}
