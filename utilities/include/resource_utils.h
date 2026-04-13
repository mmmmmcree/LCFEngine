#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace lcf {
    class ResourceControlBlock
    {
    public:
        using deleter_t = std::function<void(void)>;
    public:
        ResourceControlBlock(deleter_t deleter) : m_deleter(std::move(deleter)) {}
        ~ResourceControlBlock() = default;
        ResourceControlBlock(const ResourceControlBlock &) = delete;
        ResourceControlBlock & operator=(const ResourceControlBlock &) = delete;
        ResourceControlBlock(ResourceControlBlock &&) = delete;
        ResourceControlBlock & operator=(ResourceControlBlock &&) = delete;
    public:
        void increaseRefCount() noexcept { m_strong_count.fetch_add(1, std::memory_order_relaxed); }
        bool decreaseRefCountAndShouldDestroy() noexcept { return m_strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1; }
        bool tryIncrementStrongCount() noexcept
        {
            uint32_t expected = m_strong_count.load(std::memory_order_relaxed);
            while (expected != 0) {
                if (m_strong_count.compare_exchange_weak(expected, expected + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }
        uint32_t getStrongCount() const noexcept { return m_strong_count.load(std::memory_order_acquire); }
        void destroyResource() noexcept { m_deleter(); }
        void increaseWeakRefCount() noexcept { m_weak_count.fetch_add(1, std::memory_order_relaxed); }
        bool decreaseWeakRefCountAndShouldDelete() noexcept { return m_weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1; }
    private:
        deleter_t m_deleter;
        std::atomic<uint32_t> m_strong_count { 0u };
        std::atomic<uint32_t> m_weak_count { 0u };
    };

    class ResourceLease
    {
    public:
        ResourceLease() = default;
        explicit ResourceLease(ResourceControlBlock * control_block_p) : m_control_block_p(control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ~ResourceLease() noexcept { this->tryDestroy(); }
        ResourceLease(const ResourceLease & other) noexcept : m_control_block_p(other.m_control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ResourceLease & operator=(const ResourceLease & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_control_block_p = other.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
            return *this;
        }
        ResourceLease(ResourceLease && other) noexcept :
            m_control_block_p(std::exchange(other.m_control_block_p, nullptr))
        {
        }
        ResourceLease & operator=(ResourceLease && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            return *this;
        }
    private:
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            if (m_control_block_p->decreaseRefCountAndShouldDestroy()) {
                m_control_block_p->destroyResource();
                if (m_control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                    delete m_control_block_p;
                }
                m_control_block_p = nullptr;
            }
        }
    private:
        ResourceControlBlock * m_control_block_p = nullptr;
    };

    template <typename Resource>
    class ResourceWeakPointer;

    template <typename Resource>
    class ResourcePointer
    {
        template <typename R>
        friend class ResourceWeakPointer;
    public:
        using deleter_t = std::function<void(Resource *)>;
    public:
        ResourcePointer() = default;
        ResourcePointer(Resource * resource_p) { this->create(resource_p); }
        ResourcePointer(Resource * resource_p, deleter_t deleter) { this->create(resource_p, std::move(deleter)); }
        ResourcePointer(Resource && resource) { this->create(new Resource(std::forward<Resource>(resource))); }
        ResourcePointer(Resource && resource, deleter_t deleter) { this->create(new Resource(std::forward<Resource>(resource)), std::move(deleter)); }
        ResourcePointer(std::unique_ptr<Resource> resource_up) { this->create(resource_up.release()); }
        ~ResourcePointer() noexcept { this->tryDestroy(); }
        ResourcePointer(const ResourcePointer & other) noexcept { this->copyFrom(other); }
        ResourcePointer & operator=(const ResourcePointer & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->copyFrom(other);
            return *this;
        }
        ResourcePointer & operator=(Resource && resource)
        {
            return this->operator=(ResourcePointer(std::forward<Resource>(resource)));
        }
        ResourcePointer & operator=(std::unique_ptr<Resource> resource_up)
        {
            return this->operator=(ResourcePointer(resource_up.release()));
        }
        ResourcePointer(ResourcePointer && other) noexcept :
            m_resource_p(std::exchange(other.m_resource_p, nullptr)),
            m_control_block_p(std::exchange(other.m_control_block_p, nullptr))
        {
        }
        ResourcePointer & operator=(ResourcePointer && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_resource_p = std::exchange(other.m_resource_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            return *this;
        }
        Resource * operator->() const { return m_resource_p; }
        Resource & operator*() const { return *m_resource_p; }
        operator bool() const noexcept { return m_resource_p; }
        ResourceLease lease() const noexcept { return ResourceLease(m_control_block_p); }
        Resource * & get() noexcept { return m_resource_p; }
        const Resource * & get() const noexcept { return m_resource_p; }
    private:
        void create(Resource * resource_p)
        {
            m_resource_p = resource_p;
            m_control_block_p = new ResourceControlBlock([resource_p = m_resource_p]() { delete resource_p; });
            if (m_control_block_p) {
                m_control_block_p->increaseRefCount();
                m_control_block_p->increaseWeakRefCount();
            }
        }
        void create(Resource * resource_p, deleter_t deleter)
        {
            m_resource_p = resource_p;
            m_control_block_p = new ResourceControlBlock([deleter=std::move(deleter), resource_p = m_resource_p]() { deleter(resource_p); });
            if (m_control_block_p) {
                m_control_block_p->increaseRefCount();
                m_control_block_p->increaseWeakRefCount();
            }
        }
        void copyFrom(const ResourcePointer & other) noexcept
        {
            m_resource_p = other.m_resource_p;
            m_control_block_p = other.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            if (m_control_block_p->decreaseRefCountAndShouldDestroy()) {
                m_control_block_p->destroyResource();
                m_resource_p = nullptr;
                if (m_control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                    delete m_control_block_p;
                }
                m_control_block_p = nullptr;
            }
        }
    private:
        Resource * m_resource_p = nullptr;
        ResourceControlBlock * m_control_block_p = nullptr;
    };

    template <typename Resource>
    class ResourceWeakPointer
    {
    public:
        ResourceWeakPointer() = default;
        ResourceWeakPointer(const ResourcePointer<Resource> & strong) noexcept
            : m_resource_p(strong.m_resource_p), m_control_block_p(strong.m_control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseWeakRefCount(); }
        }
        ~ResourceWeakPointer() noexcept { this->releaseWeak(); }
        ResourceWeakPointer(const ResourceWeakPointer & other) noexcept
            : m_resource_p(other.m_resource_p), m_control_block_p(other.m_control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseWeakRefCount(); }
        }
        ResourceWeakPointer & operator=(const ResourceWeakPointer & other) noexcept
        {
            if (this == &other) { return *this; }
            this->releaseWeak();
            m_resource_p = other.m_resource_p;
            m_control_block_p = other.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseWeakRefCount(); }
            return *this;
        }
        ResourceWeakPointer(ResourceWeakPointer && other) noexcept
            : m_resource_p(std::exchange(other.m_resource_p, nullptr)),
              m_control_block_p(std::exchange(other.m_control_block_p, nullptr))
        {
        }
        ResourceWeakPointer & operator=(ResourceWeakPointer && other) noexcept
        {
            if (this == &other) { return *this; }
            this->releaseWeak();
            m_resource_p = std::exchange(other.m_resource_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
            return *this;
        }
        ResourceWeakPointer & operator=(const ResourcePointer<Resource> & strong) noexcept
        {
            this->releaseWeak();
            m_resource_p = strong.m_resource_p;
            m_control_block_p = strong.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseWeakRefCount(); }
            return *this;
        }
        operator bool() const noexcept { return not expired(); }
    public:
        ResourcePointer<Resource> lock() const noexcept
        {
            if (not m_control_block_p or not m_control_block_p->tryIncrementStrongCount()) { return {}; }
            ResourcePointer<Resource> result;
            result.m_resource_p = m_resource_p;
            result.m_control_block_p = m_control_block_p;
            return result;
        }
        bool expired() const noexcept
        {
            if (not m_control_block_p) { return true; }
            return m_control_block_p->getStrongCount() == 0;
        }
        void reset() noexcept
        {
            this->releaseWeak();
            m_resource_p = nullptr;
            m_control_block_p = nullptr;
        }
    private:
        void releaseWeak() noexcept
        {
            if (not m_control_block_p) { return; }
            if (m_control_block_p->decreaseWeakRefCountAndShouldDelete()) {
                delete m_control_block_p;
            }
            m_resource_p = nullptr;
            m_control_block_p = nullptr;
        }
    private:
        Resource * m_resource_p = nullptr;
        ResourceControlBlock * m_control_block_p = nullptr;
    };

    template <typename Resource, typename... Args>
    ResourcePointer<Resource> make_resource_ptr(Args &&... args)
    {
        return ResourcePointer<Resource>(new Resource(std::forward<Args>(args)...));
    }

    template <typename Resource, typename... Args>
    ResourcePointer<Resource> make_resource_ptr_with_deleter(typename ResourcePointer<Resource>::deleter_t deleter, Args &&... args)
    {
        return ResourcePointer<Resource>(new Resource(std::forward<Args>(args)...), std::move(deleter));
    }
}

namespace std {
    template <typename T>
    struct hash<lcf::ResourcePointer<T>>
    {
        size_t operator()(const lcf::ResourcePointer<T> & ptr) const noexcept
        {
            return std::hash<T *>()(ptr.get());
        }
    };
}