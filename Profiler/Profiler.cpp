#include "Profiler.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>

#if PROFILER_ENABLE

namespace
{
constexpr size_t kMaxDomainDepth = 16;

struct AllocationHeader
{
    MemoryDomain domain;
};

// Thread-local bounded stack of memory domains for proper nesting support
thread_local std::array<MemoryDomain, kMaxDomainDepth> g_memoryDomainStack;
thread_local size_t g_memoryDomainDepth = 0;

// Domain name mapping
const char* GetMemoryDomainName(MemoryDomain id)
{
    switch (id)
    {
    case MemoryDomain::Default:
        return "Default";
    case MemoryDomain::Particles:
        return "Particles";
    case MemoryDomain::Animation:
        return "Animation";
    case MemoryDomain::Simulation:
        return "Simulation";
    case MemoryDomain::Ui:
        return "Ui";
    default:
        return "Unknown";
    }
}

MemoryDomain GetCurrentDomain()
{
    if (g_memoryDomainDepth == 0)
        return MemoryDomain::Default;
    return g_memoryDomainStack[g_memoryDomainDepth - 1];
}

constexpr size_t kDefaultAlignment = alignof(std::max_align_t);

size_t PrefixSize(size_t alignment)
{
    size_t meta = sizeof(AllocationHeader) + sizeof(size_t);
    return (meta + alignment - 1) & ~(alignment - 1);
}

void* AllocateImpl(std::size_t count, size_t alignment)
{
    size_t prefixSize = PrefixSize(alignment);
    auto* base = (char*)malloc(prefixSize + count);

    auto* header = (AllocationHeader*)base;
    header->domain = GetCurrentDomain();

    auto* ptr = base + prefixSize;
    *(size_t*)(ptr - sizeof(size_t)) = prefixSize;

    ProfilerTraceAllocN(ptr, count, GetMemoryDomainName(header->domain));
    ProfilerTraceAlloc(ptr, count);

    return ptr;
}

void FreeImpl(void* ptr)
{
    if (ptr)
    {
        auto* data = (char*)ptr;
        size_t offset = *(size_t*)(data - sizeof(size_t));
        auto* base = data - offset;
        auto* header = (AllocationHeader*)base;
        ProfilerTraceFreeN(ptr, GetMemoryDomainName(header->domain));
        ProfilerTraceFree(ptr);
        free(base);
    }
}
} // namespace

void PushMemoryDomain(MemoryDomain id)
{
    if (id != MemoryDomain::Default)
    {
        assert(g_memoryDomainDepth < kMaxDomainDepth);
        g_memoryDomainStack[g_memoryDomainDepth++] = id;
    }
}

void PopMemoryDomain()
{
    if (g_memoryDomainDepth > 0)
    {
        --g_memoryDomainDepth;
    }
}

MemoryDomain GetCurrentMemoryDomain()
{
    return GetCurrentDomain();
}

void* operator new(std::size_t count)
{
    return AllocateImpl(count, kDefaultAlignment);
}

void* operator new(std::size_t count, std::align_val_t alignment)
{
    size_t actual = std::max(static_cast<size_t>(alignment), kDefaultAlignment);
    return AllocateImpl(count, actual);
}

void operator delete(void* ptr) noexcept
{
    FreeImpl(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    FreeImpl(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
    FreeImpl(ptr);
}

void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept
{
    FreeImpl(ptr);
}

#endif
