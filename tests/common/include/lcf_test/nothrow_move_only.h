#pragma once

namespace lcf::test {

    // Nothrow-move-only type to exercise migrate_nothrow_move branch.
    struct NothrowMoveOnly
    {
        int value = 0;
        NothrowMoveOnly() = default;
        explicit NothrowMoveOnly(int v) noexcept : value(v) {}
        NothrowMoveOnly(const NothrowMoveOnly &) = delete;
        NothrowMoveOnly & operator=(const NothrowMoveOnly &) = delete;
        NothrowMoveOnly(NothrowMoveOnly && other) noexcept : value(other.value) { other.value = 0; }
        NothrowMoveOnly & operator=(NothrowMoveOnly && other) noexcept { value = other.value; other.value = 0; return *this; }
    };

} // namespace lcf::test
