// HLSL compute shader for squaring numbers
// Compiled to SPIR-V using DXC for Vulkan

// Use StructuredBuffer for Vulkan compatibility
[[vk::binding(0, 0)]]
StructuredBuffer<int> InputKeys: register(t0);

[[vk::binding(1, 0)]]
RWStructuredBuffer<int> OutputKeys: register(u0);

[[vk::binding(2, 0)]]
cbuffer Params
{
    uint Count;
}

groupshared uint KeysA[256];
groupshared uint KeysB[256];
// Per bucket total count for the whole group
groupshared uint Hist[16];
// Prefix sum of Hist.
// BucketOffset[b] = where bucket b starts in KeysB.
groupshared uint BucketOffset[16];
// for each wave and bucket: how many lanes in that wave belong to bucket
// 256 / 32 = 8 -> nb max de warp dispo. peut etre sous utilisé si warp de 64
groupshared uint WaveBucketCounts[8 * 16];
// prefix over waves: how many previous waves had this bucket
groupshared uint WaveBucketOffsets[8 * 16];

[numthreads(256, 1, 1)]
void main(uint3 localId: SV_GroupThreadID, uint3 gtid: SV_DispatchThreadID)
{
    uint tid = localId.x;
    uint lane = WaveGetLaneIndex();
    uint waveSize = WaveGetLaneCount();
    uint waveIndex = tid / waveSize;
    uint waveNb = 8;// 256 / waveSize;
    bool active = (tid < Count);
        

    if (active)
        KeysA[tid] = InputKeys[tid];

    GroupMemoryBarrierWithGroupSync();

    // Separate the integer in 8 parts, each one taking 4 bits, 16 values possible (2**4)
    for (uint shift = 0; shift < 32; shift += 4)
    {
        // be very careful with early return, as if Count < 16, then we miss some of the path afterward !

        if (tid < 16)
            Hist[tid] = 0;

        if (tid < 8 * 16)
        {
            WaveBucketCounts[tid] = 0;
            WaveBucketOffsets[tid] = 0;
        }

        GroupMemoryBarrierWithGroupSync();

        // Get the current part of the integer we want to use for the counting sort
        uint key = 0;
        uint bucket = 0;
        if (active)
        {
            key = KeysA[tid];
            bucket = (key >> shift) & 15;
        }

        // count the number of keys that fall in each bucket -> histogram
        // we group the add by waves
        for (uint b = 0; b < 16; b++)
        {
            uint countInWave = WaveActiveCountBits(active && (bucket == b));
            if (lane == 0)
                WaveBucketCounts[waveIndex * 16 + b] = countInWave;
        }

        GroupMemoryBarrierWithGroupSync();

        // Nombre d'element dans chaque bucket
        if (tid < 16)
        {
            uint b = tid;
            uint sum = 0;
            for (uint w = 0; w < waveNb; w++)
                sum += WaveBucketCounts[w* 16 + b];
            Hist[b] = sum;
        }

        GroupMemoryBarrierWithGroupSync();


        // On recupère l'index où chaque bucket commence
        if (tid < 16)
        {
            uint sum = 0;
            for (uint b = 0; b < tid; b++)
                sum += Hist[b];
            BucketOffset[tid] = sum;
        }

        GroupMemoryBarrierWithGroupSync();

        // Il faut maintenant trier chaque Bucket.
        // On va d'abord trier les waves en trouvant l'index où chaque wave commence dans le bucket
        if (tid < 8 * 16)
        {
            uint w = tid / 16;
            uint b = tid % 16;

            uint sum = 0;
            for (uint previousWave; previousWave < w; previousWave++)
                sum += WaveBucketCounts[previousWave * 16 + b];
            WaveBucketOffsets[w * 16 + b] = sum;
        }

        GroupMemoryBarrierWithGroupSync();
        // Maintenant qu'on a l'offset du debut de la wave, on peut trier a l'intérieur de la wave elle meme
        // On va chercher a trouver l'offset de chaque element de la wave.
        uint rankInSameBucket = 0;
        for (uint b = 0; b < 16; ++b)
        {
            uint r = WavePrefixCountBits(active && bucket == b);
            if (bucket == b)
                rankInSameBucket = r;

        }

        // Maintenant qu'on a tout, il faut tout combiner pour obtenir l'offset final de notre key dans l'output
        if (active)
        {
            uint dst = BucketOffset[bucket] + WaveBucketOffsets[waveIndex + 16 + bucket] + rankInSameBucket;
            KeysB[dst] = key;
        }
        GroupMemoryBarrierWithGroupSync();

        if (active)
            KeysA[tid] = KeysB[tid];

        GroupMemoryBarrierWithGroupSync();
    }
    if (active)
        OutputKeys[tid] = KeysA[tid];
}
