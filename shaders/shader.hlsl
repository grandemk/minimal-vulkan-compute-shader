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

groupshared uint Hist[16];
groupshared uint BucketOffset[16];

[numthreads(256, 1, 1)]
void main(uint3 localId: SV_GroupThreadID, uint3 gtid: SV_DispatchThreadID)
{
    uint tid = localId.x;

    // Load input keys into shared memory
    uint key = 0xffffffff;
    if (tid < Count)
        key = InputKeys[tid];

    KeysA[tid] = key;

    // After this, KeysA is commited for all threads
    GroupMemoryBarrierWithGroupSync();

    for (uint shift = 0; shift < 32; shift += 4)
    {
        // Initialize histogram with 16 buckets.
        // 16 threads will be used to initialize the histogram.
        // so a warp of 32 threads will be used to initialize the histogram.
        if (tid < 16)
            Hist[tid] = 0;
        
        // After this, the histogram is commited to 0 for all threads
        GroupMemoryBarrierWithGroupSync();

        if (tid < Count)
        {
            // Add one occurence in the histogram for the current 'digit'
            uint bucket = (KeysA[tid] >> shift) & 0xf;
            InterlockedAdd(Hist[bucket], 1);
        }

        // After this, the histogram is commited for all threads
        GroupMemoryBarrierWithGroupSync();

        // Prefix sum histogram, done by first 16 threads
        if (tid < 16)
        {
            uint sum = 0;
            for (uint b = 0; b < tid; b++)
            {
                sum += Hist[b];
            }
            BucketOffset[tid] = sum;
        }
        // Now we got the offset where the numbers with the digit tid will be placed at the end

        // After this BucketOffset is commited for all threads
        GroupMemoryBarrierWithGroupSync();

        // Local rank inside a bucket
        if (tid < Count)
        {
            uint key = KeysA[tid];
            uint bucket = (key >> shift) & 0xf;
            uint rank = 0;
            // Count previous keys in same bucket
            // triangular sum, first thread doesn't do anything.
            // last thread does N time the work
            for (uint j = 0; j < tid; j++)
            {
                uint otherBucket = (KeysA[j] >> shift) & 0xf;
                if (otherBucket == bucket)
                    rank++;
            }
            // Place the key in the current pass order in a stable manner
            uint dst = BucketOffset[bucket] + rank;
            KeysB[dst] = key;
        }

        // After this, KeysB is commited for all threads
        GroupMemoryBarrierWithGroupSync();

        // Swap for next pass
        if (tid < Count)
            KeysA[tid] = KeysB[tid];
    }

    // Store final result
    if (tid < Count)
        OutputKeys[tid] = KeysA[tid];

}
