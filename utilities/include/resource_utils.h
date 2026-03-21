#pragma once

#include <atomic>
#include <functional>

namespace lcf {
    class ResourceControlBlock
    {
    public:
        using deleter_t = std::function<void(void)>;
    public:
        ResourceControlBlock(deleter_t deleter) : m_deleter(std::move(deleter)) {}
        ~ResourceControlBlock() { m_deleter(); }
        ResourceControlBlock(const ResourceControlBlock &) = delete;
        ResourceControlBlock & operator=(const ResourceControlBlock &) = delete;
        ResourceControlBlock(ResourceControlBlock &&) = default;
        ResourceControlBlock & operator=(ResourceControlBlock &&) = default;
    public:
        void increaseRefCount() noexcept { m_ref_count.fetch_add(1, std::memory_order_relaxed); }
        bool decreaseRefCountAndShouldDestroy() noexcept { return m_ref_count.fetch_sub(1, std::memory_order_relaxed) == 1; }
    private:
        deleter_t m_deleter;
        std::atomic<uint32_t> m_ref_count { 0u };
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
                delete m_control_block_p;
                m_control_block_p = nullptr;
            }
        }
    private:
        ResourceControlBlock * m_control_block_p = nullptr;
    };

    template <typename Resource>
    class ResourcePointer
    {
    public:
        using deleter_t = std::function<void(Resource *)>;
    public:
        ResourcePointer() = default;
        ResourcePointer(Resource * resource_p) :
            m_resource_p(resource_p),
            m_control_block_p(new ResourceControlBlock([resource_p = m_resource_p]() { delete resource_p; }))
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ResourcePointer(Resource * resource_p, deleter_t deleter) :
            m_resource_p(resource_p),
            m_control_block_p(new ResourceControlBlock([deleter=std::move(deleter), resource_p = m_resource_p]() { deleter(resource_p); }))
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ~ResourcePointer() noexcept { this->tryDestroy(); }
        ResourcePointer(const ResourcePointer & other) noexcept : 
            m_resource_p(other.m_resource_p),
            m_control_block_p(other.m_control_block_p)
        {
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
        }
        ResourcePointer & operator=(const ResourcePointer & other) noexcept
        {
            if (this == &other) { return *this; }
            this->tryDestroy();
            m_resource_p = other.m_resource_p;
            m_control_block_p = other.m_control_block_p;
            if (m_control_block_p) { m_control_block_p->increaseRefCount(); }
            return *this;
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
        ResourceLease getLease() const noexcept { return ResourceLease(m_control_block_p); }
    private:
        void tryDestroy() noexcept
        {
            if (not m_control_block_p) { return; }
            if (m_control_block_p->decreaseRefCountAndShouldDestroy()) {
                delete m_control_block_p;
                m_resource_p = nullptr;
                m_control_block_p = nullptr;
            }
        }
    private:
        Resource * m_resource_p = nullptr;
        ResourceControlBlock * m_control_block_p = nullptr;
    };
    namespace details {
        template <typename Resource, typename Tuple, std::size_t... Indexes>
        ResourcePointer<Resource> make_resource_ptr_with_tail_deleter(Tuple && args_tuple, std::index_sequence<Indexes...>)
        {
            using deleter_t = typename ResourcePointer<Resource>::deleter_t;
            constexpr auto last_index = std::tuple_size_v<std::remove_reference_t<Tuple>> - 1;
            deleter_t deleter(std::get<last_index>(std::forward<Tuple>(args_tuple)));
            return ResourcePointer<Resource>(
                new Resource(std::get<Indexes>(std::forward<Tuple>(args_tuple))...),
                std::move(deleter)
            );
        }
    }

    template <typename Resource, typename... Args>
    ResourcePointer<Resource> make_resource_ptr(Args &&... args)
    {
        using deleter_t = typename ResourcePointer<Resource>::deleter_t;
        if constexpr (sizeof...(Args) > 0 and
            std::is_convertible_v<std::tuple_element_t<sizeof...(Args) - 1, std::tuple<std::decay_t<Args>...>>, deleter_t>
        ) {
            auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
            return details::make_resource_ptr_with_tail_deleter<Resource>(
                std::move(args_tuple),
                std::make_index_sequence<sizeof...(Args) - 1> {}
            );
        } else {
            return ResourcePointer<Resource>(new Resource(std::forward<Args>(args)...));
        }
    }
}