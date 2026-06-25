#include <vulkan/vulkan.hpp>
#include <iostream>
#include <fstream>
#include <functional>
#include <vector>
#include <stack>

#include "Profiler.h"
#include "ProfilerDll.h"


//////////////////////////////////////////////////////////////////////////
//                              CONTEXT                                 //
//////////////////////////////////////////////////////////////////////////

struct TContext
{
    // Instance & Device
    vk::Instance Instance;
    vk::PhysicalDevice PhysicalDevice;
    vk::Device Device;
    uint32_t ComputeQueueFamilyIndex = uint32_t(-1);
    vk::Queue Queue;

    // Buffers & Memory
    vk::Buffer InBuffer;
    vk::Buffer OutBuffer;
    vk::Buffer ParamsBuffer;
    vk::DeviceMemory InBufferMemory;
    vk::DeviceMemory OutBufferMemory;
    vk::DeviceMemory ParamsBufferMemory;

    // Pipeline
    vk::ShaderModule ShaderModule;
    vk::DescriptorSetLayout DescriptorSetLayout;
    vk::PipelineLayout PipelineLayout;
    vk::PipelineCache PipelineCache;
    vk::Pipeline ComputePipeline;

    // Descriptor Sets
    vk::DescriptorPool DescriptorPool;
    vk::DescriptorSet DescriptorSet;

    // Command Buffer
    vk::CommandPool CommandPool;
    vk::CommandBuffer CmdBuffer;

    // Synchronization
    vk::Fence Fence;

    // Tracy Vulkan context
    vk::CommandPool TracyCommandPool;
    vk::CommandBuffer TracyCmdBuffer;
    ProfilerVkCtx ProfilerCtx = nullptr;

    // Buffer size info
    uint32_t NumElements = 10;
    uint32_t BufferSize = 0;
};

using DestroyFunc = std::function<void()>;
std::stack<DestroyFunc> g_DestroyStack;


//////////////////////////////////////////////////////////////////////////
//                          SUBROUTINES                                 //
//////////////////////////////////////////////////////////////////////////

void CreateInstance(TContext& Ctx)
{
    ZoneScoped;

    vk::ApplicationInfo AppInfo{
        "VulkanCompute",      // Application Name
        1,                    // Application Version
        nullptr,              // Engine Name or nullptr
        0,                    // Engine Version
        VK_API_VERSION_1_1    // Vulkan API version
    };

    const std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };
    vk::InstanceCreateInfo InstanceCreateInfo(
            vk::InstanceCreateFlags(), // Flags
            &AppInfo,                  // Application Info
            Layers.size(),             // Layers count
            Layers.data()              // Layers
            );
    Ctx.Instance = vk::createInstance(InstanceCreateInfo);

    g_DestroyStack.push([&Ctx]() {
        Ctx.Instance.destroy();
    });
}

void SelectPhysicalDevice(TContext& Ctx)
{
    ZoneScoped;

    Ctx.PhysicalDevice = Ctx.Instance.enumeratePhysicalDevices().front();
    vk::PhysicalDeviceProperties DeviceProps = Ctx.PhysicalDevice.getProperties();
    std::cout << "Device Name    : " << DeviceProps.deviceName << std::endl;
    const uint32_t ApiVersion = DeviceProps.apiVersion;
    std::cout << "Vulkan Version : " << VK_VERSION_MAJOR(ApiVersion) << "." 
              << VK_VERSION_MINOR(ApiVersion) << "." << VK_VERSION_PATCH(ApiVersion) << std::endl;
}

void FindQueueFamily(TContext& Ctx)
{
    ZoneScoped;

    std::vector<vk::QueueFamilyProperties> QueueFamilyProps = Ctx.PhysicalDevice.getQueueFamilyProperties();
    auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties& Prop) {
        return Prop.queueFlags & vk::QueueFlagBits::eCompute;
    });
    Ctx.ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
    std::cout << "Compute Queue Family Index: " << Ctx.ComputeQueueFamilyIndex << std::endl;
}

void CreateDevice(TContext& Ctx)
{
    ZoneScoped;

    float queuePriorities = 1.0f;
    vk::DeviceQueueCreateInfo DeviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(),   // Flags
            Ctx.ComputeQueueFamilyIndex,    // Queue Family Index
            1,                              // Number of Queues
            &queuePriorities
            );
    vk::DeviceCreateInfo DeviceCreateInfo(
            vk::DeviceCreateFlags(),        // Flags
            1,
            &DeviceQueueCreateInfo          // Device Queue Create Info struct
            );
    Ctx.Device = Ctx.PhysicalDevice.createDevice(DeviceCreateInfo);
    Ctx.Queue = Ctx.Device.getQueue(Ctx.ComputeQueueFamilyIndex, 0);

    g_DestroyStack.push([&Ctx]() {
        Ctx.Device.destroy();
    });
}

void CreateTracyVulkanContext(TContext& Ctx)
{
    ZoneScoped;

    // Create a dedicated command pool and buffer for Tracy's initialization.
    // Tracy needs a command buffer to initialize its query pool and do calibration.
    // After initialization, Tracy won't touch this command buffer again
    // (unless TracyVkCollect is called, which we don't use).
    vk::CommandPoolCreateInfo TracyCmdPoolInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        Ctx.ComputeQueueFamilyIndex);
    Ctx.TracyCommandPool = Ctx.Device.createCommandPool(TracyCmdPoolInfo);

    vk::CommandBufferAllocateInfo TracyCmdBufInfo(
        Ctx.TracyCommandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto TracyCmdBufs = Ctx.Device.allocateCommandBuffers(TracyCmdBufInfo);
    Ctx.TracyCmdBuffer = TracyCmdBufs.front();

    // Create Tracy Vulkan context.
    // Tracy will use the command buffer for initialization (reset query pool, write timestamps, etc.)
    // and then return it to the initial state (it calls vkQueueWaitIdle at the end).
    Ctx.ProfilerCtx = ProfilerVkContext(
        static_cast<VkPhysicalDevice>(Ctx.PhysicalDevice),
        static_cast<VkDevice>(Ctx.Device),
        static_cast<VkQueue>(Ctx.Queue),
        static_cast<VkCommandBuffer>(Ctx.TracyCmdBuffer));

    ProfilerVkContextName(Ctx.ProfilerCtx, "Compute", 7);

    g_DestroyStack.push([&Ctx]() {
        if (Ctx.ProfilerCtx) {
            ProfilerVkDestroy(Ctx.ProfilerCtx);
            Ctx.ProfilerCtx = nullptr;
        }
        Ctx.Device.destroyCommandPool(Ctx.TracyCommandPool);
    });
}

void CreateBuffers(TContext& Ctx)
{
    ZoneScoped;

    const uint32_t NumElements = Ctx.NumElements;
    const uint32_t Count = NumElements;
    Ctx.BufferSize = NumElements * sizeof(int32_t);

    // Create buffers
    vk::BufferCreateInfo BufferCreateInfo{
        vk::BufferCreateFlags(),                    // Flags
        Ctx.BufferSize,                             // Size
        vk::BufferUsageFlagBits::eStorageBuffer,    // Usage
        vk::SharingMode::eExclusive,                // Sharing mode
        1,                                          // Number of queue family indices
        &Ctx.ComputeQueueFamilyIndex                // List of queue family indices
    };
    Ctx.InBuffer = Ctx.Device.createBuffer(BufferCreateInfo);
    Ctx.OutBuffer = Ctx.Device.createBuffer(BufferCreateInfo);

    vk::BufferCreateInfo ParamsBufferCreateInfo{
        vk::BufferCreateFlags(),                    // Flags
        sizeof(uint32_t),                           // Size
        vk::BufferUsageFlagBits::eUniformBuffer,    // Usage
        vk::SharingMode::eExclusive,                // Sharing mode
        1,                                          // Number of queue family indices
        &Ctx.ComputeQueueFamilyIndex                // List of queue family indices
    };
    Ctx.ParamsBuffer = Ctx.Device.createBuffer(ParamsBufferCreateInfo);

    // Memory requirements
    vk::MemoryRequirements InBufferMemoryRequirements = Ctx.Device.getBufferMemoryRequirements(Ctx.InBuffer);
    vk::MemoryRequirements OutBufferMemoryRequirements = Ctx.Device.getBufferMemoryRequirements(Ctx.OutBuffer);
    vk::MemoryRequirements ParamsBufferMemoryRequirements = Ctx.Device.getBufferMemoryRequirements(Ctx.ParamsBuffer);

    // Query memory properties
    vk::PhysicalDeviceMemoryProperties MemoryProperties = Ctx.PhysicalDevice.getMemoryProperties();

    uint32_t MemoryTypeIndex = uint32_t(~0);
    vk::DeviceSize MemoryHeapSize = uint32_t(~0);
    for (uint32_t CurrentMemoryTypeIndex = 0; CurrentMemoryTypeIndex < MemoryProperties.memoryTypeCount; ++CurrentMemoryTypeIndex)
    {
        vk::MemoryType MemoryType = MemoryProperties.memoryTypes[CurrentMemoryTypeIndex];
        if ((vk::MemoryPropertyFlagBits::eHostVisible & MemoryType.propertyFlags) &&
            (vk::MemoryPropertyFlagBits::eHostCoherent & MemoryType.propertyFlags))
        {
            MemoryHeapSize = MemoryProperties.memoryHeaps[MemoryType.heapIndex].size;
            MemoryTypeIndex = CurrentMemoryTypeIndex;
            break;
        }
    }

    std::cout << "Memory Type Index: " << MemoryTypeIndex << std::endl;
    std::cout << "Memory Heap Size : " << MemoryHeapSize / 1024 / 1024 / 1024 << " GB" << std::endl;

    // Allocate memory
    vk::MemoryAllocateInfo InBufferMemoryAllocateInfo(InBufferMemoryRequirements.size, MemoryTypeIndex);
    vk::MemoryAllocateInfo OutBufferMemoryAllocateInfo(OutBufferMemoryRequirements.size, MemoryTypeIndex);
    vk::MemoryAllocateInfo ParamsBufferMemoryAllocateInfo(ParamsBufferMemoryRequirements.size, MemoryTypeIndex);
    Ctx.InBufferMemory = Ctx.Device.allocateMemory(InBufferMemoryAllocateInfo);
    Ctx.OutBufferMemory = Ctx.Device.allocateMemory(OutBufferMemoryAllocateInfo);
    Ctx.ParamsBufferMemory = Ctx.Device.allocateMemory(ParamsBufferMemoryAllocateInfo);

    // Map memory and write
    int32_t* InBufferPtr = static_cast<int32_t*>(Ctx.Device.mapMemory(Ctx.InBufferMemory, 0, Ctx.BufferSize));
    for (uint32_t I = 0; I < NumElements; ++I) {
        InBufferPtr[I] = NumElements - I;
    }
    Ctx.Device.unmapMemory(Ctx.InBufferMemory);

    uint32_t* ParamsBufferPtr = static_cast<uint32_t*>(Ctx.Device.mapMemory(Ctx.ParamsBufferMemory, 0, sizeof(uint32_t)));
    *ParamsBufferPtr = Count;
    Ctx.Device.unmapMemory(Ctx.ParamsBufferMemory);

    // Bind buffers to memory
    Ctx.Device.bindBufferMemory(Ctx.InBuffer, Ctx.InBufferMemory, 0);
    Ctx.Device.bindBufferMemory(Ctx.OutBuffer, Ctx.OutBufferMemory, 0);
    Ctx.Device.bindBufferMemory(Ctx.ParamsBuffer, Ctx.ParamsBufferMemory, 0);

    g_DestroyStack.push([&Ctx]() {
        Ctx.Device.freeMemory(Ctx.InBufferMemory);
        Ctx.Device.freeMemory(Ctx.OutBufferMemory);
        Ctx.Device.freeMemory(Ctx.ParamsBufferMemory);
        Ctx.Device.destroyBuffer(Ctx.InBuffer);
        Ctx.Device.destroyBuffer(Ctx.OutBuffer);
        Ctx.Device.destroyBuffer(Ctx.ParamsBuffer);
    });
}

void CreatePipeline(TContext& Ctx)
{
    ZoneScoped;

    // Shader module
    std::vector<char> ShaderContents;
    if (std::ifstream ShaderFile{ "shaders/shader.hlsl.spv", std::ios::binary | std::ios::ate }) {
        const size_t FileSize = ShaderFile.tellg();
        ShaderFile.seekg(0);
        ShaderContents.resize(FileSize, '\0');
        ShaderFile.read(ShaderContents.data(), FileSize);
    }

    vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(),                                // Flags
        ShaderContents.size(),                                        // Code size
        reinterpret_cast<const uint32_t*>(ShaderContents.data()));    // Code
    Ctx.ShaderModule = Ctx.Device.createShaderModule(ShaderModuleCreateInfo);

    // Descriptor Set Layout
    const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
        {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
        {2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute}
    };
    vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
        vk::DescriptorSetLayoutCreateFlags(),
        DescriptorSetLayoutBinding);
    Ctx.DescriptorSetLayout = Ctx.Device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);

    // Pipeline Layout
    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), Ctx.DescriptorSetLayout);
    Ctx.PipelineLayout = Ctx.Device.createPipelineLayout(PipelineLayoutCreateInfo);
    Ctx.PipelineCache = Ctx.Device.createPipelineCache(vk::PipelineCacheCreateInfo());

    // Compute Pipeline
    vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(
        vk::PipelineShaderStageCreateFlags(),  // Flags
        vk::ShaderStageFlagBits::eCompute,     // Stage
        Ctx.ShaderModule,                      // Shader Module
        "main"                                 // Shader Entry Point
        );
    vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(
        vk::PipelineCreateFlags(),    // Flags
        PipelineShaderCreateInfo,     // Shader Create Info struct
        Ctx.PipelineLayout            // Pipeline Layout
        );
    Ctx.ComputePipeline = Ctx.Device.createComputePipeline(Ctx.PipelineCache, ComputePipelineCreateInfo).value;

    g_DestroyStack.push([&Ctx]() {
        Ctx.Device.destroyPipeline(Ctx.ComputePipeline);
        Ctx.Device.destroyPipelineCache(Ctx.PipelineCache);
        Ctx.Device.destroyPipelineLayout(Ctx.PipelineLayout);
        Ctx.Device.destroyDescriptorSetLayout(Ctx.DescriptorSetLayout);
        Ctx.Device.destroyShaderModule(Ctx.ShaderModule);
    });
}

void CreateDescriptorSets(TContext& Ctx)
{
    ZoneScoped;

    const std::vector<vk::DescriptorPoolSize> DescriptorPoolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
    };
    vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSizes);
    Ctx.DescriptorPool = Ctx.Device.createDescriptorPool(DescriptorPoolCreateInfo);

    // Allocate descriptor sets, update them to use buffers:
    vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(Ctx.DescriptorPool, 1, &Ctx.DescriptorSetLayout);
    const std::vector<vk::DescriptorSet> DescriptorSets = Ctx.Device.allocateDescriptorSets(DescriptorSetAllocInfo);
    Ctx.DescriptorSet = DescriptorSets.front();
    vk::DescriptorBufferInfo InBufferInfo(Ctx.InBuffer, 0, Ctx.NumElements * sizeof(int32_t));
    vk::DescriptorBufferInfo OutBufferInfo(Ctx.OutBuffer, 0, Ctx.NumElements * sizeof(int32_t));
    vk::DescriptorBufferInfo ParamsBufferInfo(Ctx.ParamsBuffer, 0, sizeof(uint32_t));

    const std::vector<vk::WriteDescriptorSet> WriteDescriptorSets = {
        {Ctx.DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
        {Ctx.DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
        {Ctx.DescriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &ParamsBufferInfo}
    };
    Ctx.Device.updateDescriptorSets(WriteDescriptorSets, {});

    g_DestroyStack.push([&Ctx]() {
        Ctx.Device.destroyDescriptorPool(Ctx.DescriptorPool);
    });
}

void SubmitWork(TContext& Ctx)
{
    ZoneScoped;

    // Command Pool
    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), Ctx.ComputeQueueFamilyIndex);
    Ctx.CommandPool = Ctx.Device.createCommandPool(CommandPoolCreateInfo);

    // Allocate Command buffer from Pool
    vk::CommandBufferAllocateInfo CommandBufferAllocInfo(
        Ctx.CommandPool,                         // Command Pool
        vk::CommandBufferLevel::ePrimary,        // Level
        1);                                      // Num Command Buffers
    const std::vector<vk::CommandBuffer> CmdBuffers = Ctx.Device.allocateCommandBuffers(CommandBufferAllocInfo);
    Ctx.CmdBuffer = CmdBuffers.front();

    // Record commands
    vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    Ctx.CmdBuffer.begin(CmdBufferBeginInfo);

    // GPU zone scope - must end before CmdBuffer.end() is called
    {
        ProfilerVkZone(Ctx.ProfilerCtx, static_cast<VkCommandBuffer>(Ctx.CmdBuffer), "ComputeDispatch");

        Ctx.CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, Ctx.ComputePipeline);
        Ctx.CmdBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,    // Bind point
                Ctx.PipelineLayout,                 // Pipeline Layout
                0,                                  // First descriptor set
                { Ctx.DescriptorSet },              // List of descriptor sets
                {});                                // Dynamic offsets
        Ctx.CmdBuffer.dispatch(Ctx.NumElements, 1, 1);
        // VkCtxScope destructor called here, while CmdBuffer is still recording
    }

    Ctx.CmdBuffer.end();

    // Fence and submit
    Ctx.Fence = Ctx.Device.createFence(vk::FenceCreateInfo());
    vk::SubmitInfo SubmitInfo(
            0,                // Num Wait Semaphores
            nullptr,          // Wait Semaphores
            nullptr,          // Pipeline Stage Flags
            1,                // Num Command Buffers
            &Ctx.CmdBuffer);  // List of command buffers
    Ctx.Queue.submit({ SubmitInfo }, Ctx.Fence);
    (void) Ctx.Device.waitForFences(
            { Ctx.Fence },    // List of fences
            true,             // Wait All
            uint64_t(-1));    // Timeout

    // Collect GPU timestamps from Tracy's query pool
    // This reads the timestamps and sends them to the profiler
    vk::CommandBufferBeginInfo CollectBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    Ctx.TracyCmdBuffer.begin(CollectBeginInfo);
    ProfilerVkCollect(Ctx.ProfilerCtx, static_cast<VkCommandBuffer>(Ctx.TracyCmdBuffer));
    Ctx.TracyCmdBuffer.end();
    vk::SubmitInfo CollectSubmitInfo(0, nullptr, nullptr, 1, &Ctx.TracyCmdBuffer);
    Ctx.Queue.submit({ CollectSubmitInfo });
    Ctx.Queue.waitIdle();

    // Map output buffer and read results
    int32_t* InBufferPtr = static_cast<int32_t*>(Ctx.Device.mapMemory(Ctx.InBufferMemory, 0, Ctx.BufferSize));
    std::cout << std::endl;
    std::cout << "INPUT:  ";
    for (uint32_t I = 0; I < Ctx.NumElements; ++I) {
        std::cout << InBufferPtr[I] << " ";
    }
    std::cout << std::endl;
    Ctx.Device.unmapMemory(Ctx.InBufferMemory);

    int32_t* OutBufferPtr = static_cast<int32_t*>(Ctx.Device.mapMemory(Ctx.OutBufferMemory, 0, Ctx.BufferSize));
    std::cout << "OUTPUT: ";
    for (uint32_t I = 0; I < Ctx.NumElements; ++I) {
        std::cout << OutBufferPtr[I] << " ";
    }
    std::cout << std::endl;
    Ctx.Device.unmapMemory(Ctx.OutBufferMemory);

    g_DestroyStack.push([&Ctx]() {
        Ctx.Device.destroyFence(Ctx.Fence);
        Ctx.Device.resetCommandPool(Ctx.CommandPool, vk::CommandPoolResetFlags());
        Ctx.Device.destroyCommandPool(Ctx.CommandPool);
    });
}

void Cleanup(TContext& Ctx)
{
    ZoneScoped;

    // Execute destroy lambdas in reverse order (LIFO)
    while (!g_DestroyStack.empty()) {
        g_DestroyStack.top()();
        g_DestroyStack.pop();
    }
}


//////////////////////////////////////////////////////////////////////////
//                              MAIN                                    //
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    // ── Profiler startup (manual lifetime required by TRACY_MANUAL_LIFETIME) ──
    ProfilerStartup();
    SetTracyActive(true);
    ProfilerAppInfo("MinimalVulkanCompute", 21);
    ZoneScoped;                         // CPU zone for the whole main()

    TContext Ctx;

    CreateInstance(Ctx);
    SelectPhysicalDevice(Ctx);
    FindQueueFamily(Ctx);
    CreateDevice(Ctx);
    CreateTracyVulkanContext(Ctx);
    CreateBuffers(Ctx);
    CreatePipeline(Ctx);
    CreateDescriptorSets(Ctx);
    SubmitWork(Ctx);

    FrameMark;

    Cleanup(Ctx);

    ProfilerShutdown();
    _exit(0);
}
