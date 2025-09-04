#pragma once
#include <memory>

namespace lcf {
    template <typename T, typename SharedPtr = std::shared_ptr<T>, typename UniquePtr = std::unique_ptr<T>>
    struct PointerDefsBase
    {
        using SharedPointer = SharedPtr;
        using UniquePointer = UniquePtr;
        using WeakPointer = std::weak_ptr<T>;
    };

    template <typename T>
    struct PointerDefs : public PointerDefsBase<T>
    {
        using SharedPointer = PointerDefsBase<T>::SharedPointer;
        using UniquePointer = PointerDefsBase<T>::UniquePointer;
        using WeakPointer = PointerDefsBase<T>::WeakPointer;
        template <typename ...Args>
        static SharedPointer makeShared(Args &&...args) { return std::make_shared<T>(std::forward<Args>(args)...); }
        template <typename ...Args>
        static UniquePointer makeUnique(Args &&...args) { return std::make_unique<T>(std::forward<Args>(args)...); }
    };

    /**
     * @brief Macro used when both Base and Derived classes have PointerDefs specializations.
     * struct Base : PointerDefs<Base> {... };
     * struct Derived : Base, PointerDefs<Derived>
     * {
     *      IMPORT_POINTER_DEFS(Derived);
     * };
     */
    #define IMPORT_POINTER_DEFS(T) \
        using typename PointerDefs<T>::SharedPointer; \
        using typename PointerDefs<T>::UniquePointer; \
        using typename PointerDefs<T>::WeakPointer; \
        using PointerDefs<T>::makeShared; \
        using PointerDefs<T>::makeUnique
}
