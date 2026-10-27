#ifndef TUNGOL_COMMON_BONE_COMMON
#define TUNGOL_COMMON_BONE_COMMON

struct AnimChannel 
{
    uint boneIndex;
    uint keyCount;
    uint keyIndex;
};

struct Animation 
{
    float duration;
    uint channelIndex;
    uint channelCount;
};

struct Keyframe 
{
    float time;

    vec3 position;
    vec4 rotation;
    vec3 scale;
};

struct BoneMeta 
{
    uint parentIndex;
    uint instanceIndex;
    uint localBoneIndex;
};

struct PlaybackState 
{
    float time;
    uint clipIndex;
    uint boneOffset;
    uint boneCount;
};

#endif
