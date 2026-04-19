#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

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
        uint32_t getRefCount() const noexcept { return m_strong_count.load(std::memory_order_acquire); }
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
        operator bool() const noexcept { return m_control_block_p; }
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
    class ResourceWeakPtr;

    template <typename Resource>
    class ResourcePtr
    {
        template <typename R>
        friend class ResourcePtr;
        template <typename R>
        friend class ResourceWeakPointer;
        template <typename R>
        requires (not std::is_same_v<R, Resource> and std::is_convertible_v<R *, Resource *>)
        using ConvertibleResourcePointer = ResourcePtr<R>;
    public:
        using deleter_t = std::function<void(Resource *)>;
        using resource_type = Resource;
    public:
        ResourcePtr() = default;
        ResourcePtr(Resource * resource_p) { this->create(resource_p); }
        ResourcePtr(Resource * resource_p, deleter_t deleter) { this->create(resource_p, std::move(deleter)); }
        ResourcePtr(Resource && resource) { this->create(new Resource(std::forward<Resource>(resource))); }
        ResourcePtr(Resource && resource, deleter_t deleter) { this->create(new Resource(std::forward<Resource>(resource)), std::move(deleter)); }
        ResourcePtr(std::unique_ptr<Resource> resource_up) { this->create(resource_up.release()); }
        ~ResourcePtr() noexcept { this->tryDestroy(); }
        ResourcePtr(const ResourcePtr & other) noexcept { this->copyFrom(other); }
        ResourcePtr & operator=(const ResourcePtr & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->copyFrom(other);
            return *this;
        }
        ResourcePtr(ResourcePtr && other) noexcept { this->stealFrom(std::move(other)); }
        ResourcePtr & operator=(ResourcePtr && other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            this->stealFrom(std::move(other));
            return *this;
        }
        // --- converting copy/move (e.g. ResourcePtr<T> → ResourcePtr<const T>) ---
        template <typename R>
        ResourcePtr(const ConvertibleResourcePointer<R> & other) noexcept { this->copyFrom(other); }
        template <typename R>
        ResourcePtr & operator=(const ConvertibleResourcePointer<R> & other) noexcept
        {
            this->tryDestroy();
            this->copyFrom(other);
            return *this;
        }
        template <typename R>
        ResourcePtr(ConvertibleResourcePointer<R> && other) noexcept { this->stealFrom(std::move(other)); }
        template <typename R>
        ResourcePtr & operator=(ConvertibleResourcePointer<R> && other) noexcept
        {
            this->tryDestroy();
            this->stealFrom(std::move(other));
            return *this;
        }
        ResourcePtr & operator=(Resource && resource)
        {
            return this->operator=(ResourcePtr(std::forward<Resource>(resource)));
        }
        ResourcePtr & operator=(std::unique_ptr<Resource> resource_up)
        {
            return this->operator=(ResourcePtr(resource_up.release()));
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
        template <typename R>
        requires std::is_convertible_v<R *, Resource *>
        void copyFrom(const ResourcePtr<R> & other) noexcept
        {
            m_resource_p = other.m_resource_p;
            m_control_block_p = other.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        template <typename R>
        requires std::is_convertible_v<R *, Resource *>
        void stealFrom(ResourcePtr<R> && other) noexcept
        {
            m_resource_p = std::exchange(other.m_resource_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
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
    class ResourceWeakPtr
    {
        template <typename R>
        friend class ResourceWeakPointer;
        template <typename R>
        requires (not std::is_same_v<R, Resource> and std::is_convertible_v<R *, Resource *>)
        using ConvertibleResourcePointer = ResourcePtr<R>;
        template <typename R>
        requires (not std::is_same_v<R, Resource> and std::is_convertible_v<R *, Resource *>)
        using ConvertibleResourceWeakPointer = ResourceWeakPtr<R>;
    public:
        using resource_type = Resource;
    public:
        ResourceWeakPtr() = default;
        ~ResourceWeakPtr() noexcept { this->releaseWeak(); }
        // --- from ResourcePtr (same-type + converting) ---
        ResourceWeakPtr(const ResourcePtr<Resource> & strong) noexcept { this->copyFrom(strong.m_resource_p, strong.m_control_block_p); }
        ResourceWeakPtr & operator=(const ResourcePtr<Resource> & strong) noexcept
        {
            this->releaseWeak();
            this->copyFrom(strong.m_resource_p, strong.m_control_block_p);
            return *this;
        }
        template <typename R>
        ResourceWeakPtr(const ConvertibleResourcePointer<R> & strong) noexcept { this->copyFrom(strong.m_resource_p, strong.m_control_block_p); }
        template <typename R>
        ResourceWeakPtr & operator=(const ConvertibleResourcePointer<R> & strong) noexcept
        {
            this->releaseWeak();
            this->copyFrom(strong.m_resource_p, strong.m_control_block_p);
            return *this;
        }
        // --- same-type copy/move (non-template, required by the standard) ---
        ResourceWeakPtr(const ResourceWeakPtr & other) noexcept { this->copyFrom(other.m_resource_p, other.m_control_block_p); }
        ResourceWeakPtr & operator=(const ResourceWeakPtr & other) noexcept
        {
            if (this == &other) { return *this; }
            this->releaseWeak();
            this->copyFrom(other.m_resource_p, other.m_control_block_p);
            return *this;
        }
        ResourceWeakPtr(ResourceWeakPtr && other) noexcept { this->stealFrom(other); }
        ResourceWeakPtr & operator=(ResourceWeakPtr && other) noexcept
        {
            if (this == &other) { return *this; }
            this->releaseWeak();
            this->stealFrom(other);
            return *this;
        }
        // --- converting copy/move from ResourceWeakPointer<R> ---
        template <typename R>
        ResourceWeakPtr(const ConvertibleResourceWeakPointer<R> & other) noexcept { this->copyFrom(other.m_resource_p, other.m_control_block_p); }
        template <typename R>
        ResourceWeakPtr & operator=(const ConvertibleResourceWeakPointer<R> & other) noexcept
        {
            this->releaseWeak();
            this->copyFrom(other.m_resource_p, other.m_control_block_p);
            return *this;
        }
        template <typename R>
        ResourceWeakPtr(ConvertibleResourceWeakPointer<R> && other) noexcept { this->stealFrom(other); }
        template <typename R>
        ResourceWeakPtr & operator=(ConvertibleResourceWeakPointer<R> && other) noexcept
        {
            this->releaseWeak();
            this->stealFrom(other);
            return *this;
        }
        operator bool() const noexcept { return not expired(); }
    public:
        ResourcePtr<Resource> lock() const noexcept
        {
            if (not m_control_block_p or not m_control_block_p->tryIncrementStrongCount()) { return {}; }
            ResourcePtr<Resource> result;
            result.m_resource_p = m_resource_p;
            result.m_control_block_p = m_control_block_p;
            return result;
        }
        bool expired() const noexcept
        {
            if (not m_control_block_p) { return true; }
            return m_control_block_p->getRefCount() == 0;
        }
        void reset() noexcept
        {
            this->releaseWeak();
        }
    private:
        template <typename R>
        requires std::is_convertible_v<R *, Resource *>
        void copyFrom(R * resource_p, ResourceControlBlock * control_block_p) noexcept
        {
            m_resource_p = resource_p;
            m_control_block_p = control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseWeakRefCount(); }
        }
        template <typename R>
        requires std::is_convertible_v<R *, Resource *>
        void stealFrom(ResourceWeakPtr<R> & other) noexcept
        {
            m_resource_p = std::exchange(other.m_resource_p, nullptr);
            m_control_block_p = std::exchange(other.m_control_block_p, nullptr);
        }
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

    template <typename ResourcePointerLike, typename Resource>
    concept resource_pointer_like_c = requires(ResourcePointerLike rp) {
        { rp.lease() } -> std::convertible_to<ResourceLease>;
        { *rp } -> std::convertible_to<Resource &>;
    };

    template <typename Resource>
    class ResourceStrongRef;

    template <typename Resource>
    class ResourceRef
    {
        template <typename R>
        friend class ResourceRef;
        template <typename R>
        requires (not std::is_same_v<R, Resource> and std::is_convertible_v<R &, Resource &>)
        using ConvertibleResourceRef = ResourceRef<R>;
        template <typename R>
        friend class ResourceStrongRef;
    public:
        ResourceRef(Resource & resource, const ResourceLease & lease = {}) :
            m_resource(resource),
            m_lease(lease)
        {}
        template <typename R>
        ResourceRef(const ConvertibleResourceRef<R> & other) :
            m_resource(other.m_resource),
            m_lease(other.m_lease)
        {}
        ResourceRef(resource_pointer_like_c<Resource> auto && rp) :
            m_resource(*rp),
            m_lease(rp.lease())
        {}
        ResourceRef(const ResourceRef & other) noexcept = default;
        ResourceRef(ResourceRef && other) noexcept = default;
        ResourceRef & operator=(const ResourceRef & other) noexcept = default;
        ResourceRef & operator=(ResourceRef && other) noexcept = default;
        Resource & operator*() const noexcept { return m_resource; }
        Resource * operator->() const noexcept { return &m_resource; }
    public:
        bool isStrongRef() const noexcept { return m_lease; }
    private:
        const ResourceLease & lease() const noexcept { return m_lease; }
    private:
        Resource & m_resource;
        ResourceLease m_lease;
    };

    template <typename Resource>
    class ResourceStrongRef
    {
        template <typename R>
        friend class ResourceStrongRef;
        template <typename R>
        requires (not std::is_same_v<R, Resource> and std::is_convertible_v<R &, Resource &>)
        using ConvertibleResourceStrongRef = ResourceStrongRef<R>;
    public:
        ResourceStrongRef(Resource & resource, const ResourceLease & lease) :
            m_resource(resource),
            m_lease(lease)
        {
            this->checkStrongRef();
        }
        template <typename R>
        ResourceStrongRef(const ConvertibleResourceStrongRef<R> & other) :
            m_resource(other.m_resource),
            m_lease(other.m_lease)
        {
            this->checkStrongRef();
        }
        ResourceStrongRef(resource_pointer_like_c<Resource> auto && rp) :
            m_resource(*rp),
            m_lease(rp.lease())
        {
            this->checkStrongRef();
        }
        ResourceStrongRef(const ResourceStrongRef & other) noexcept = default;
        ResourceStrongRef(ResourceStrongRef && other) noexcept = default;
        ResourceStrongRef & operator=(const ResourceStrongRef & other) noexcept = default;
        ResourceStrongRef & operator=(ResourceStrongRef && other) noexcept = default;
        Resource & operator*() const noexcept { return m_resource; }
        Resource * operator->() const noexcept { return &m_resource; }
    private:
        void checkStrongRef() const
        {
            if (not m_lease) { throw std::logic_error("not a strong reference"); }    
        }
    private:
        Resource & m_resource;
        ResourceLease m_lease;
    };

    template <typename Resource, typename... Args>
    ResourcePtr<Resource> make_resource_ptr(Args &&... args)
    {
        return ResourcePtr<Resource>(new Resource(std::forward<Args>(args)...));
    }

    template <typename Resource, typename... Args>
    ResourcePtr<Resource> make_resource_ptr_with_deleter(typename ResourcePtr<Resource>::deleter_t deleter, Args &&... args)
    {
        return ResourcePtr<Resource>(new Resource(std::forward<Args>(args)...), std::move(deleter));
    }
}

namespace std {
    template <typename T>
    struct hash<lcf::ResourcePtr<T>>
    {
        size_t operator()(const lcf::ResourcePtr<T> & ptr) const noexcept
        {
            return std::hash<T *>()(ptr.get());
        }
    };
}