#include <resource_utils.h>
#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace {

TEST(WeakResourceLease, EmptyLeaseIsExpired)
{
    lcf::WeakResourceLease weak;
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak);
    EXPECT_FALSE(weak.lock());
    weak.reset();
    lcf::ResourceLease empty;
    EXPECT_TRUE(lcf::WeakResourceLease(empty).isExpired());
    lcf::ResourceWeakPtr<int> empty_ptr;
    EXPECT_TRUE(empty_ptr.getWeakLease().isExpired());
}

TEST(WeakResourceLease, ObservationDoesNotRetainResource)
{
    int destroyed = 0;
    auto owner = lcf::make_resource_ptr_with_deleter<int>([&](int * value) {
        ++destroyed;
        delete value;
    }, 42);
    auto strong = owner.lease();
    lcf::WeakResourceLease weak(strong);
    EXPECT_EQ(strong.getRefCount(), 2u);
    EXPECT_FALSE(weak.isExpired());
    owner = lcf::ResourcePtr<int> {};
    auto locked = weak.lock();
    ASSERT_TRUE(locked);
    EXPECT_EQ(locked.getRefCount(), 2u);
    strong = {};
    EXPECT_EQ(destroyed, 0);
    locked = {};
    EXPECT_EQ(destroyed, 1);
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak.lock());
}

TEST(WeakResourceLease, WeakPointerExportsIndependentObserver)
{
    auto owner = lcf::make_resource_ptr<int>(42);
    auto strong = owner.lease();
    lcf::ResourceWeakPtr<int> typed(owner);
    auto weak = typed.getWeakLease();
    EXPECT_EQ(strong.getRefCount(), 2u);
    typed.reset();
    owner = lcf::ResourcePtr<int> {};
    EXPECT_FALSE(weak.isExpired());
    strong = {};
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak.lock());
}

TEST(WeakResourceLease, ExpiredPointerCanExportObserver)
{
    auto marker = std::make_shared<int>(0);
    std::weak_ptr<int> control_block_marker = marker;
    // The deleter capture lives until the resource control block is deleted.
    auto owner = lcf::make_resource_ptr_with_deleter<int>([marker](int * value) {
        delete value;
    }, 42);
    marker.reset();
    lcf::ResourceWeakPtr<int> typed(owner);
    owner = lcf::ResourcePtr<int> {};
    auto weak = typed.getWeakLease();
    typed.reset();
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak.lock());
    EXPECT_FALSE(control_block_marker.expired());
    weak.reset();
    EXPECT_TRUE(control_block_marker.expired());
}

TEST(WeakResourceLease, CopyMoveAndAssignmentPreserveWeakOwnership)
{
    auto marker = std::make_shared<int>(0);
    std::weak_ptr<int> control_block_marker = marker;
    auto owner = lcf::make_resource_ptr_with_deleter<int>([marker](int * value) {
        delete value;
    }, 42);
    marker.reset();
    lcf::WeakResourceLease original(owner.lease());
    auto copy = original;
    auto moved = std::move(copy);
    EXPECT_TRUE(copy.isExpired());
    lcf::WeakResourceLease assigned;
    assigned = original;
    original.reset();
    assigned = std::move(moved);
    EXPECT_TRUE(moved.isExpired());
    auto & self = assigned;
    assigned = self;
    assigned = std::move(self);
    EXPECT_FALSE(assigned.isExpired());
    owner = lcf::ResourcePtr<int> {};
    EXPECT_TRUE(assigned.isExpired());
    EXPECT_FALSE(control_block_marker.expired());
    assigned = lcf::WeakResourceLease {};
    EXPECT_TRUE(control_block_marker.expired());
}

TEST(WeakResourceLease, AssignmentReleasesPreviousControlBlock)
{
    auto marker = std::make_shared<int>(0);
    std::weak_ptr<int> control_block_marker = marker;
    auto first = lcf::make_resource_ptr_with_deleter<int>([marker](int * value) {
        delete value;
    }, 1);
    marker.reset();
    lcf::WeakResourceLease weak(first.lease());
    first = lcf::ResourcePtr<int> {};
    auto second = lcf::make_resource_ptr<int>(2);
    lcf::WeakResourceLease next(second.lease());
    weak = next;
    EXPECT_TRUE(control_block_marker.expired());
    EXPECT_FALSE(weak.isExpired());
    second = lcf::ResourcePtr<int> {};
    EXPECT_TRUE(weak.isExpired());
}

TEST(WeakResourceLease, LockRacesWithLastStrongRelease)
{
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::atomic<int> destroyed {0};
        std::atomic<bool> started {false};
        auto owner = lcf::make_resource_ptr_with_deleter<int>([&](int * value) {
            ++destroyed;
            delete value;
        }, 42);
        lcf::WeakResourceLease weak(owner.lease());
        std::thread worker([&] {
            auto initial = weak.lock();
            started.store(true, std::memory_order_release);
            initial = {};
            for (int attempt = 0; attempt < 100; ++attempt) {
                auto lease = weak.lock();
                if (lease) { EXPECT_EQ(destroyed.load(), 0); }
            }
        });
        while (not started.load(std::memory_order_acquire)) { std::this_thread::yield(); }
        owner = lcf::ResourcePtr<int> {};
        worker.join();
        EXPECT_EQ(destroyed.load(), 1);
        EXPECT_TRUE(weak.isExpired());
        EXPECT_FALSE(weak.lock());
    }
}

} // namespace
