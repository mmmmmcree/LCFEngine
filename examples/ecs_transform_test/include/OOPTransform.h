#pragma once
#include "Matrix.h"
#include <vector>
#include "Entity.h"

namespace lcf {
    class OOPTransform
    {
        using Self = OOPTransform;
    public:
        using ChildrenPtrList = std::vector<Self *>;
        OOPTransform()
        {
            m_children.reserve(4);
        }
        void attachTo(OOPTransform * parent) noexcept
        {
            m_parent = parent;
            parent->m_children.emplace_back(this);
        }
        void translateLocal(const Vector3D<float> &translation) noexcept
        {
            if (translation.isNull()) { return; }
            m_local_matrix.translateLocal(translation);
            this->markDirty();
        }
        const Matrix4x4 & getLocalMatrix() const noexcept { return m_local_matrix; }
        const Matrix4x4 & getPlainWorldMatrix() const noexcept { return m_world_matrix; }
        const Matrix4x4 & getWorldMatrix() const noexcept
        {
            if (not m_is_dirty) { return m_world_matrix; }
            m_is_dirty = false;
            return m_world_matrix = m_parent ? m_parent->getWorldMatrix() * this->getLocalMatrix() : this->getLocalMatrix();
        }
        void markDirty()
        {
            if (m_is_dirty) { return; }
            m_is_dirty = true;
            for (auto child : m_children) {
                child->markDirty();
            }
        }
    private:
        mutable bool m_is_dirty = true;
        Self * m_parent = nullptr;
        ChildrenPtrList m_children;
        Matrix4x4 m_local_matrix;
        mutable Matrix4x4 m_inverted_world_matrix;
        mutable Matrix4x4 m_world_matrix;
    };

    class OOPTransform2
    {
        using Self = OOPTransform2;
    public:
        using ChildrenPtrList = std::vector<Self *>;
        OOPTransform2() = default;
        void attachTo(Self * parent) noexcept
        {
            m_parent = parent;
        }
        void translateLocal(const Vector3D<float> &translation) noexcept
        {
            if (translation.isNull()) { return; }
            m_local_matrix.translateLocal(translation);
        }
        const Matrix4x4 & getLocalMatrix() const noexcept { return m_local_matrix; }
        const Matrix4x4 & getWorldMatrix() const noexcept
        {
            if (not m_is_dirty) { return m_world_matrix; }
            m_is_dirty = false;
            return m_world_matrix = m_parent ? m_parent->getWorldMatrix() * this->getLocalMatrix() : this->getLocalMatrix();
        }
        const Matrix4x4 & getPlainWorldMatrix() const noexcept { return m_world_matrix; }
    public:
        mutable bool m_is_dirty = true;
        Self * m_parent = nullptr;
        Matrix4x4 m_local_matrix;
        mutable Matrix4x4 m_world_matrix;
    };

    struct OOPTransform2Hierarchy
    {
        using ChildrenList = std::vector<EntityHandle>;
        ChildrenList m_children;
    };
}