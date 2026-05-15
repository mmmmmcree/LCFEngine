#pragma once

namespace lcf::test {

    // Copy-constructible but throwing on copy/move (no noexcept). Forces the
    // migrate_checked branch in container reallocation paths.
    struct ThrowingCopy
    {
        int value = 0;
        ThrowingCopy() = default;
        explicit ThrowingCopy(int v) : value(v) {}
        ThrowingCopy(const ThrowingCopy & other) : value(other.value) {}
        ThrowingCopy(ThrowingCopy && other) : value(other.value) {}
        ThrowingCopy & operator=(const ThrowingCopy & other) { value = other.value; return *this; }
        ThrowingCopy & operator=(ThrowingCopy && other) { value = other.value; return *this; }
    };

} // namespace lcf::test
