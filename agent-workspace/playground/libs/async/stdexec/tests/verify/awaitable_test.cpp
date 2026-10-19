#include <async/execution.h>
#include <Runtime.h>
#include <gtest/gtest.h>
#include <map>
#include <stdexcept>

namespace {

namespace runtime = lcf::async::execution;
namespace stde = stdexec;

/*
 * Objective: an unstarted coroutine owns its parameters and releases its selected resource.
 * Input: a frame resource and a shared parameter with no other owner.
 * Procedure: construct the lazy task in a scope, then discard it outside that scope.
 * Expected: the body never runs, the parameter expires, and allocations balance.
 * Failure hints: inspect initial_suspend, Awaitable destruction, and the frame resource header.
 */
void unstarted_frame_releases_ownership();
TEST(Awaitable, UnstartedFrameReleasesOwnership) { unstarted_frame_releases_ownership(); }

/*
 * Objective: nested allocation scopes restore the outer resource without controlling execution.
 * Input: two resources and three lazy value tasks.
 * Procedure: create tasks in outer, inner, and restored outer scopes; run after both scopes end.
 * Expected: values 20, 21, 22, with balanced resource-specific size/alignment pairs.
 * Failure hints: inspect FrameAllocatorScope restoration and deallocate_frame resource selection.
 */
void nested_frame_scopes_restore_resource();
TEST(Awaitable, NestedFrameScopesRestoreResource) { nested_frame_scopes_restore_resource(); }

/*
 * Objective: frame release uses its original resource on a different execution thread.
 * Input: a task created with a resource supporting concurrent deallocation.
 * Procedure: end the allocation scope, then sync_wait on a joined worker thread.
 * Expected: value 42 and exactly balanced allocations on the original resource.
 * Failure hints: inspect frame headers rather than the worker's active allocation scope.
 */
void frame_resource_survives_thread_switch();
TEST(Awaitable, FrameResourceSurvivesThreadSwitch) { frame_resource_survives_thread_switch(); }

/*
 * Objective: default frames remain valid after their creator thread's cache has been destroyed.
 * Input: a lazy task moved out of a temporary creator thread.
 * Procedure: join the creator, then consume the task on the current thread.
 * Expected: value 42 with intact frame ownership.
 * Failure hints: inspect FrameCache thread exit and release into the destroying thread's cache.
 */
void default_frame_outlives_creator_thread();
TEST(Awaitable, DefaultFrameOutlivesCreatorThread) { default_frame_outlives_creator_thread(); }

/*
 * Objective: a thread-local task can release its frame after that thread's cache destructor.
 * Input: a task holder initialized before the first frame allocation on a fresh thread.
 * Procedure: leave the task in the holder and join the thread, forcing reverse destruction order.
 * Expected: the unstarted body stays inactive and its shared parameter expires.
 * Failure hints: inspect FrameCache::is_retired and the direct-delete exit path.
 */
void default_frame_releases_after_cache_retirement();
TEST(Awaitable, DefaultFrameReleasesAfterCacheRetirement) { default_frame_releases_after_cache_retirement(); }

/*
 * Objective: a failing frame allocation never enters the coroutine and restores the outer scope.
 * Input: a normal outer resource and a throwing inner resource.
 * Procedure: invoke an opaque inner factory to prevent frame elision, then execute an outer task.
 * Expected: bad_alloc, no inner body invocation, and a valid outer result.
 * Failure hints: inspect promise operator new and allocation scope unwinding.
 */
void frame_allocation_failure_restores_scope();
TEST(Awaitable, FrameAllocationFailureRestoresScope) { frame_allocation_failure_restores_scope(); }

/*
 * Objective: throwing coroutine bodies release the original frame allocation.
 * Input: a lazy task that throws a system_error after entering its body.
 * Procedure: create under a custom resource and sync_wait after its scope ends.
 * Expected: the original error code and balanced frame ownership.
 * Failure hints: inspect promise exception storage and TaskAwaiter::await_resume cleanup.
 */
void exceptional_frame_returns_to_resource();
TEST(Awaitable, ExceptionalFrameReturnsToResource) { exceptional_frame_returns_to_resource(); }

/*
 * Objective: stopped propagates across nested tasks while destroying suspended frame parameters.
 * Input: a parent and child task, a shared lifetime marker, and just_stopped().
 * Procedure: sync_wait the parent and observe its marker after completion.
 * Expected: stopped without resuming either body after the await; marker expires.
 * Failure hints: inspect with_awaitable_senders continuation unwinding and awaiter destruction.
 */
void stopped_nested_frames_release_parameters();
TEST(Awaitable, StoppedNestedFramesReleaseParameters) { stopped_nested_frames_release_parameters(); }

/*
 * Objective: own Awaitable and raw exec::task interoperate in both await directions.
 * Input: own and native tasks returning values and awaiting one another.
 * Procedure: consume both compositions directly through sync_wait.
 * Expected: each composition yields 42.
 * Failure hints: inspect as_awaitable, sender completion affinity, and parent context binding.
 */
void native_tasks_interoperate();
TEST(Awaitable, NativeTasksInteroperate) { native_tasks_interoperate(); }

/*
 * Objective: an over-aligned return object remains correctly aligned inside the promise.
 * Input: an alignas(64) value that records its alignment on every move.
 * Procedure: return and sync_wait the value with a custom frame resource.
 * Expected: value 42, valid alignment, and matching resource deallocation.
 * Failure hints: inspect promise alignment, frame prefix padding, and takeValue moves.
 */
void over_aligned_result_keeps_alignment();
TEST(Awaitable, OverAlignedResultKeepsAlignment) { over_aligned_result_keeps_alignment(); }

class TrackingResource : public std::pmr::memory_resource
{
    using AllocationMap = std::map<void *, std::pair<std::size_t, std::size_t>>;
public:
    bool isBalanced() const noexcept { return m_allocations == m_deallocations and m_blocks.empty() and m_valid; }
    unsigned getAllocations() const noexcept { return m_allocations; }
private:
    void * do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        auto * memory_p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        const std::lock_guard lock {m_mutex};
        m_blocks.emplace(memory_p, std::pair {bytes, alignment});
        ++m_allocations;
        return memory_p;
    }
    void do_deallocate(void * memory_p, std::size_t bytes, std::size_t alignment) override
    {
        {
            const std::lock_guard lock {m_mutex};
            const auto block_it = m_blocks.find(memory_p);
            m_valid = m_valid and block_it != m_blocks.end();
            if (block_it != m_blocks.end()) {
                m_valid = m_valid and block_it->second == std::pair {bytes, alignment};
                m_blocks.erase(block_it);
            }
            ++m_deallocations;
        }
        std::pmr::new_delete_resource()->deallocate(memory_p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override { return this == &other; }
private:
    std::mutex m_mutex;
    AllocationMap m_blocks;
    unsigned m_allocations {0};
    unsigned m_deallocations {0};
    bool m_valid {true};
};

class ThrowingResource : public std::pmr::memory_resource
{
    void * do_allocate(std::size_t, std::size_t) override { throw std::bad_alloc(); }
    void do_deallocate(void *, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override { return this == &other; }
};

runtime::Awaitable<int> make_value(int value, bool & entered)
{
    entered = true;
    co_return value;
}

runtime::Awaitable<void> hold_parameter(std::shared_ptr<int> value_sp, bool & entered)
{
    entered = true;
    (void)value_sp;
    co_return;
}

void unstarted_frame_releases_ownership()
{
    TrackingResource resource;
    bool entered = false;
    auto value_sp = std::make_shared<int>(42);
    const std::weak_ptr<int> value_wp = value_sp;
    std::optional<runtime::Awaitable<void>> task_opt;
    {
        const runtime::FrameAllocatorScope scope {resource};
        task_opt.emplace(hold_parameter(std::move(value_sp), entered));
    }
    EXPECT_FALSE(entered);
    EXPECT_FALSE(value_wp.expired());
    EXPECT_EQ(resource.getAllocations(), 1u);
    task_opt.reset();
    EXPECT_FALSE(entered);
    EXPECT_TRUE(value_wp.expired());
    EXPECT_TRUE(resource.isBalanced());
}

void nested_frame_scopes_restore_resource()
{
    TrackingResource outer;
    TrackingResource inner;
    bool entered = false;
    std::optional<runtime::Awaitable<int>> first_opt;
    std::optional<runtime::Awaitable<int>> second_opt;
    std::optional<runtime::Awaitable<int>> third_opt;
    {
        const runtime::FrameAllocatorScope outer_scope {outer};
        first_opt.emplace(make_value(20, entered));
        {
            const runtime::FrameAllocatorScope inner_scope {inner};
            second_opt.emplace(make_value(21, entered));
        }
        third_opt.emplace(make_value(22, entered));
    }
    EXPECT_FALSE(entered);
    EXPECT_EQ(outer.getAllocations(), 2u);
    EXPECT_EQ(inner.getAllocations(), 1u);
    const auto first = stde::sync_wait(std::move(*first_opt));
    const auto second = stde::sync_wait(std::move(*second_opt));
    const auto third = stde::sync_wait(std::move(*third_opt));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    EXPECT_EQ(std::get<0>(*first), 20);
    EXPECT_EQ(std::get<0>(*second), 21);
    EXPECT_EQ(std::get<0>(*third), 22);
    EXPECT_TRUE(outer.isBalanced());
    EXPECT_TRUE(inner.isBalanced());
}

void frame_resource_survives_thread_switch()
{
    TrackingResource resource;
    bool entered = false;
    std::optional<runtime::Awaitable<int>> task_opt;
    {
        const runtime::FrameAllocatorScope scope {resource};
        task_opt.emplace(make_value(42, entered));
    }
    int value = 0;
    std::jthread worker {[task = std::move(*task_opt), &value]() mutable {
        const auto result = stde::sync_wait(std::move(task));
        ASSERT_TRUE(result);
        value = std::get<0>(*result);
    }};
    worker.join();
    EXPECT_TRUE(entered);
    EXPECT_EQ(value, 42);
    EXPECT_EQ(resource.getAllocations(), 1u);
    EXPECT_TRUE(resource.isBalanced());
}

void default_frame_outlives_creator_thread()
{
    bool entered = false;
    std::optional<runtime::Awaitable<int>> task_opt;
    std::jthread creator {[&] { task_opt.emplace(make_value(42, entered)); }};
    creator.join();
    EXPECT_FALSE(entered);
    const auto result = stde::sync_wait(std::move(*task_opt));
    ASSERT_TRUE(result);
    EXPECT_EQ(std::get<0>(*result), 42);
    EXPECT_TRUE(entered);
}

void default_frame_releases_after_cache_retirement()
{
    struct LateHolder
    {
        LateHolder() noexcept {}
        std::optional<runtime::Awaitable<void>> m_task_opt;
    };
    bool entered = false;
    auto value_sp = std::make_shared<int>(42);
    const std::weak_ptr<int> value_wp = value_sp;
    std::jthread creator {[value_sp = std::move(value_sp), &entered]() mutable {
        thread_local LateHolder holder;
        holder.m_task_opt.emplace(hold_parameter(std::move(value_sp), entered));
    }};
    creator.join();
    EXPECT_FALSE(entered);
    EXPECT_TRUE(value_wp.expired());
}

void frame_allocation_failure_restores_scope()
{
    TrackingResource outer;
    ThrowingResource failing;
    bool entered = false;
    const runtime::FrameAllocatorScope outer_scope {outer};
    {
        const runtime::FrameAllocatorScope inner_scope {failing};
        runtime::Awaitable<int> (* volatile factory)(int, bool &) = &make_value;
        EXPECT_THROW((void)factory(1, entered), std::bad_alloc);
    }
    EXPECT_FALSE(entered);
    const auto result = stde::sync_wait(make_value(42, entered));
    ASSERT_TRUE(result);
    EXPECT_EQ(std::get<0>(*result), 42);
    EXPECT_EQ(outer.getAllocations(), 1u);
    EXPECT_TRUE(outer.isBalanced());
}

runtime::Awaitable<int> fail_value()
{
    throw std::system_error(std::make_error_code(std::errc::permission_denied));
    co_return 0;
}

void exceptional_frame_returns_to_resource()
{
    TrackingResource resource;
    std::optional<runtime::Awaitable<int>> task_opt;
    {
        const runtime::FrameAllocatorScope scope {resource};
        task_opt.emplace(fail_value());
    }
    try { (void)stde::sync_wait(std::move(*task_opt)); FAIL() << "Expected system_error"; }
    catch (const std::system_error & error) { EXPECT_EQ(error.code(), std::errc::permission_denied); }
    EXPECT_TRUE(resource.isBalanced());
}

runtime::Awaitable<void> stopped_child(std::shared_ptr<int> value_sp, unsigned & after_await)
{
    co_await stde::just_stopped();
    ++after_await;
    (void)value_sp;
}

runtime::Awaitable<void> stopped_parent(std::shared_ptr<int> value_sp, unsigned & after_await)
{
    co_await stopped_child(std::move(value_sp), after_await);
    ++after_await;
}

void stopped_nested_frames_release_parameters()
{
    auto value_sp = std::make_shared<int>(42);
    const std::weak_ptr<int> value_wp = value_sp;
    unsigned after_await = 0;
    EXPECT_FALSE(stde::sync_wait(stopped_parent(std::move(value_sp), after_await)));
    EXPECT_EQ(after_await, 0u);
    EXPECT_TRUE(value_wp.expired());
}

exec::task<int> native_leaf() { co_return 20; }
runtime::Awaitable<int> own_parent() { co_return 22 + co_await native_leaf(); }
exec::task<int> native_parent() { co_return co_await own_parent(); }

void native_tasks_interoperate()
{
    const auto own = stde::sync_wait(own_parent());
    const auto native = stde::sync_wait(native_parent());
    ASSERT_TRUE(own);
    ASSERT_TRUE(native);
    EXPECT_EQ(std::get<0>(*own), 42);
    EXPECT_EQ(std::get<0>(*native), 42);
}

struct alignas(64) AlignedValue
{
    explicit AlignedValue(int value) noexcept : m_value(value), m_valid(reinterpret_cast<std::uintptr_t>(this) % 64 == 0) {}
    AlignedValue(AlignedValue && other) noexcept :
        m_value(other.m_value), m_valid(other.m_valid and reinterpret_cast<std::uintptr_t>(this) % 64 == 0) {}
    int m_value;
    bool m_valid;
};

runtime::Awaitable<AlignedValue> aligned_value() { co_return AlignedValue {42}; }

void over_aligned_result_keeps_alignment()
{
    TrackingResource resource;
    const runtime::FrameAllocatorScope scope {resource};
    const auto result = stde::sync_wait(aligned_value());
    ASSERT_TRUE(result);
    EXPECT_EQ(std::get<0>(*result).m_value, 42);
    EXPECT_TRUE(std::get<0>(*result).m_valid);
    EXPECT_TRUE(resource.isBalanced());
}

}
