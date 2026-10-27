#version 450

void main()
{
    const vec2 positions[3] = {
        vec2( 0.0, -0.5), // 0: Top-Center
        vec2(-0.5,  0.5), // 1: Bottom-Left
        vec2( 0.5,  0.5)  // 2: Bottom-Right
    };

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}

void main()
{
    uint boneId = gl_GlobalInvocationID.x;
    if (boneId >= pc.totalBones) return;
    locals[boneId] = bindPoses[boneId];   // always bind pose
}

void main()
{
    uint boneId = gl_GlobalInvocationID.x;

    // Bound 1: global invocation
    if (boneId >= pc.totalBones) return;
    if (boneId >= MAX_BONES) { locals[boneId] = bindPoses[boneId]; return; }

    // Bound 2: bone meta
    BoneMeta m = meta[boneId];
    if (m.instanceIndex >= MAX_INSTANCES) { locals[boneId] = bindPoses[boneId]; return; }

    PlaybackState p = pb[m.instanceIndex];

    // Bound 3: playback sanity
    if (p.boneCount == 0u) { locals[boneId] = bindPoses[boneId]; return; }
    if (p.clipIndex == UINT32_MAX) { locals[boneId] = bindPoses[boneId]; return; }

    Animation anim = anims[p.clipIndex];

    // Bound 4: animation sanity
    if (anim.channelCount == 0u) { locals[boneId] = bindPoses[boneId]; return; }
    if (anim.channelCount > MAX_CHANNELS_PER_CLIP) { locals[boneId] = bindPoses[boneId]; return; }

    // Bounded search: hard cap at MAX_CHANNELS_PER_CLIP, never unbounded
    int chanIdx = -1;
    uint searchCount = min(anim.channelCount, MAX_CHANNELS_PER_CLIP);
    for (uint i = 0u; i < searchCount; ++i)
    {
        uint c = anim.channelIndex + i;
        if (chans[c].boneIndex == m.localBoneIndex)
        {
            chanIdx = int(c);
            break;
        }
    }
    if (chanIdx < 0) { locals[boneId] = bindPoses[boneId]; return; }

    AnimChannel ch = chans[chanIdx];

    // Bound 5: channel sanity
    if (ch.keyCount == 0u) { locals[boneId] = bindPoses[boneId]; return; }
    if (ch.keyCount > MAX_KEYS_PER_CHANNEL) { locals[boneId] = bindPoses[boneId]; return; }

    uint first = ch.keyIndex;
    uint last  = ch.keyIndex + ch.keyCount - 1u;

    // Single key
    if (ch.keyCount == 1u)
    {
        Keyframe k = keys[first];
        locals[boneId] = composeTRS(k.position, k.rotation, k.scale);
        return;
    }

    // Before first key
    if (p.time <= keys[first].time)
    {
        Keyframe k = keys[first];
        locals[boneId] = composeTRS(k.position, k.rotation, k.scale);
        return;
    }

    // After last key
    if (p.time >= keys[last].time)
    {
        Keyframe k = keys[last];
        locals[boneId] = composeTRS(k.position, k.rotation, k.scale);
        return;
    }

    // Bounded bracket search — capped at MAX_KEYS_PER_CHANNEL
    uint scanCount = min(ch.keyCount - 1u, MAX_KEYS_PER_CHANNEL - 1u);
    for (uint i = 0u; i < scanCount; ++i)
    {
        uint k0 = ch.keyIndex + i;
        uint k1 = k0 + 1u;

        if (p.time < keys[k1].time)
        {
            Keyframe a = keys[k0];
            Keyframe b = keys[k1];

            float span = b.time - a.time;
            float t = (span > EPSILON) ? (p.time - a.time) / span : 0.0;

            vec3 pos = mix(a.position, b.position, t);
            vec4 rot = nlerp(a.rotation, b.rotation, t);
            vec3 scl = mix(a.scale, b.scale, t);

            locals[boneId] = composeTRS(pos, rot, scl);
            return;
        }
    }

    // Fallback if nothing matched (shouldn't happen, but safe)
    locals[boneId] = bindPoses[boneId];
}
