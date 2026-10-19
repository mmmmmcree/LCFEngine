#include "allocation_probe.h"
#include <cstdlib>
#include <limits>
#include <new>

namespace {

struct Header
{
    void * m_base_p;
    std::size_t m_size;
    std::uint64_t m_generation;
};

std::atomic_uint64_t k_generation = 0;
std::atomic_bool k_enabled = false;
std::atomic_uint64_t k_allocations = 0;
std::atomic_uint64_t k_bytes = 0;
std::atomic_uint64_t k_live = 0;
std::atomic_uint64_t k_peak = 0;

void * allocate(std::size_t size, std::size_t alignment)
{
    alignment = alignment < alignof(Header) ? alignof(Header) : alignment;
    const auto payload = size == 0 ? 1 : size;
    if (payload > std::numeric_limits<std::size_t>::max() - sizeof(Header) - alignment) { throw std::bad_alloc(); }
    void * base_p = nullptr;
    while (not (base_p = std::malloc(payload + sizeof(Header) + alignment))) {
        const auto handler = std::get_new_handler();
        if (not handler) { throw std::bad_alloc(); }
        handler();
    }
    const auto address = (reinterpret_cast<std::uintptr_t>(base_p) + sizeof(Header) + alignment - 1) & ~(alignment - 1);
    auto * header_p = reinterpret_cast<Header *>(address) - 1;
    *header_p = {base_p, size, 0};
    if (k_enabled.load(std::memory_order_relaxed)) {
        header_p->m_generation = k_generation.load(std::memory_order_relaxed);
        k_allocations.fetch_add(1, std::memory_order_relaxed);
        k_bytes.fetch_add(size, std::memory_order_relaxed);
        const auto live = k_live.fetch_add(size, std::memory_order_relaxed) + size;
        auto peak = k_peak.load(std::memory_order_relaxed);
        while (peak < live and not k_peak.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {}
    }
    return reinterpret_cast<void *>(address);
}

void deallocate(void * memory_p) noexcept
{
    if (not memory_p) { return; }
    const auto * header_p = static_cast<Header *>(memory_p) - 1;
    if (header_p->m_generation != 0 and header_p->m_generation == k_generation.load(std::memory_order_relaxed)) {
        k_live.fetch_sub(header_p->m_size, std::memory_order_relaxed);
    }
    std::free(header_p->m_base_p);
}

}

namespace lcf::async::comparison::allocation {

void begin() noexcept
{
    k_enabled = false;
    ++k_generation;
    k_allocations = 0;
    k_bytes = 0;
    k_live = 0;
    k_peak = 0;
    k_enabled = true;
}

Snapshot end() noexcept
{
    k_enabled = false;
    return {k_allocations.load(), k_bytes.load(), k_peak.load(), k_live.load()};
}

}

void * operator new(std::size_t size) { return allocate(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__); }
void * operator new[](std::size_t size) { return ::operator new(size); }
void * operator new(std::size_t size, std::align_val_t alignment) { return allocate(size, static_cast<std::size_t>(alignment)); }
void * operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void * operator new(std::size_t size, const std::nothrow_t &) noexcept { try { return ::operator new(size); } catch (...) { return nullptr; } }
void * operator new[](std::size_t size, const std::nothrow_t & tag) noexcept { return ::operator new(size, tag); }
void * operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept { try { return ::operator new(size, alignment); } catch (...) { return nullptr; } }
void * operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t & tag) noexcept { return ::operator new(size, alignment, tag); }
void operator delete(void * memory_p) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p) noexcept { deallocate(memory_p); }
void operator delete(void * memory_p, std::size_t) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p, std::size_t) noexcept { deallocate(memory_p); }
void operator delete(void * memory_p, std::align_val_t) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p, std::align_val_t) noexcept { deallocate(memory_p); }
void operator delete(void * memory_p, std::size_t, std::align_val_t) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p, std::size_t, std::align_val_t) noexcept { deallocate(memory_p); }
void operator delete(void * memory_p, const std::nothrow_t &) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p, const std::nothrow_t &) noexcept { deallocate(memory_p); }
void operator delete(void * memory_p, std::align_val_t, const std::nothrow_t &) noexcept { deallocate(memory_p); }
void operator delete[](void * memory_p, std::align_val_t, const std::nothrow_t &) noexcept { deallocate(memory_p); }
