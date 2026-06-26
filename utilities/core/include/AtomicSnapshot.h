#pragma once

#include <atomic>
#include <memory>
#include <concepts>
#include <utility>
#include "type_traits/member_pointer_traits.h"

namespace lcf {

// A lock-free single-value container. Multiple providers publish; any number of
// consumers read the latest complete snapshot. Reads never block writes and
// observe a consistent value (never a half-updated one). The held shared_ptr is
// an implementation detail; consumers see a Snapshot guard that keeps the value
// alive while in use.
template <typename T>
requires std::copy_constructible<T>
class AtomicSnapshot
{
    using ConstSP = std::shared_ptr<const T>;
public:
    class Snapshot
    {
        friend class AtomicSnapshot;
        explicit Snapshot(ConstSP value) noexcept : m_value(std::move(value)) {}
    public:
        Snapshot() noexcept = default;
        explicit operator bool() const noexcept { return static_cast<bool>(m_value); }
        const T & operator*() const noexcept { return *m_value; }
        const T * operator->() const noexcept { return m_value.get(); }
        bool operator==(const Snapshot & other) const noexcept { return m_value == other.m_value; }
    public:
        const T & value() const noexcept { return *m_value; }
    private:
        ConstSP m_value;
    };
public:
    ~AtomicSnapshot() noexcept = default;
    AtomicSnapshot() noexcept = default;
    explicit AtomicSnapshot(T initial) noexcept : m_value(std::make_shared<const T>(std::move(initial))) {}
    AtomicSnapshot(const AtomicSnapshot & other) noexcept(std::is_nothrow_copy_constructible_v<T>) = default;
    AtomicSnapshot(AtomicSnapshot && other) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
    AtomicSnapshot & operator=(const AtomicSnapshot & other) noexcept(std::is_nothrow_copy_assignable_v<T>) = default;
    AtomicSnapshot & operator=(AtomicSnapshot && other) noexcept(std::is_nothrow_move_assignable_v<T>) = default;
public:
    Snapshot load() const noexcept { return Snapshot(m_value.load(std::memory_order_acquire)); }
    void update(T value) noexcept { m_value.store(std::make_shared<const T>(std::move(value)), std::memory_order_release); }
    template <auto MemberPtr>
    requires std::default_initializable<T>
    void update(member_pointer_member_t<MemberPtr> value) noexcept
    {
        auto current = m_value.load(std::memory_order_acquire);
        while (true) {
            auto next = std::make_shared<T>(current ? *current : T{});
            (*next).*MemberPtr = std::move(value);
            if constexpr (std::equality_comparable<T>) {
                if (current and *next == *current) { return; }
            }
            if (m_value.compare_exchange_weak(
                current,
                ConstSP(std::move(next)),
                std::memory_order_release, std::memory_order_acquire)) { return; }
        }
    }
private:
    std::atomic<ConstSP> m_value;
};

// AtomicSnapshot plus a single-consumer cursor remembering the last value it
// consumed. Lets one consumer ask "changed since I last looked?" and latch the
// new value. Providers publish concurrently as with AtomicSnapshot; the consume
// side is single-threaded (the cursor is not synchronized across consumers).
template <typename T>
requires std::copy_constructible<T>
class LatchedSnapshot
{
    using Source = AtomicSnapshot<T>;
public:
    using Snapshot = typename Source::Snapshot;
public:
    ~LatchedSnapshot() noexcept = default;
    LatchedSnapshot() noexcept = default;
    explicit LatchedSnapshot(T initial) noexcept : m_source(std::move(initial)) {}
    LatchedSnapshot(const LatchedSnapshot & other) noexcept(std::is_nothrow_copy_constructible_v<T>) = default;
    LatchedSnapshot(LatchedSnapshot && other) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
    LatchedSnapshot & operator=(const LatchedSnapshot & other) noexcept(std::is_nothrow_copy_assignable_v<T>) = default;
    LatchedSnapshot & operator=(LatchedSnapshot && other) noexcept(std::is_nothrow_move_assignable_v<T>) = default;
public:
    void update(T value) noexcept { m_source.update(std::move(value)); }
    template <auto MemberPtr>
    requires std::default_initializable<T>
    void update(member_pointer_member_t<MemberPtr> value) noexcept { m_source.template update<MemberPtr>(std::move(value)); }
    Snapshot load() const noexcept { return m_source.load(); }
    Snapshot read() const noexcept { return m_consumed; }
    Snapshot loadIfChanged() noexcept
    {
        Snapshot latest = m_source.load();
        if (latest == m_consumed) { return {}; }
        m_consumed = latest;
        return latest;
    }
private:
    Source m_source = Source {T{}};
    Snapshot m_consumed;
};

} // namespace lcf
