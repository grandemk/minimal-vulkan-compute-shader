// HLSL compute shader for squaring numbers
// Compiled to SPIR-V using DXC for Vulkan

// Use StructuredBuffer for Vulkan compatibility
[[vk::binding(0, 0)]]
StructuredBuffer<int> inBuf : register(t0);

[[vk::binding(1, 0)]]
RWStructuredBuffer<int> outBuf : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint id = DTid.x;
    
    // Load integer from input buffer
    int inputValue = inBuf[id];
    
    // Square it and store in output buffer
    outBuf[id] = inputValue * inputValue;
}
