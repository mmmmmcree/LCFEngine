#pragma once
#include <memory>

namespace lcf {
    template <typename T>
    struct STDPointerDefs
    {
        using ValueType = T;
        using SharedPointer = std::shared_ptr<T>;
        using SharedConstPointer = std::shared_ptr<const T>;
        using UniquePointer = std::unique_ptr<T>;
        using UniqueConstPointer = std::unique_ptr<const T>;
        using WeakPointer = std::weak_ptr<T>;
        using WeakConstPointer = std::weak_ptr<const T>;
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
    #define IMPORT_POINTER_DEFS(PointerDefs) \
        using typename PointerDefs::SharedPointer; \
        using typename PointerDefs::UniquePointer; \
        using typename PointerDefs::WeakPointer; \
        using PointerDefs::makeShared; \
        using PointerDefs::makeUnique
}
