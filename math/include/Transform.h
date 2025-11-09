#pragma once

#include "Matrix.h"
#include "Vector.h"
#include "Quaternion.h"
#include "PointerDefs.h"
#include <vector>

namespace lcf {
    class Transform
    {
    public:
        Transform() = default;
        Transform(const Transform &other);
        Transform &operator=(const Transform &other);
        void setLocalMatrix(const Matrix4x4 & matrix) noexcept;
        void setWorldMatrix(const Matrix4x4 * world_matrix) noexcept; // controlled by TransformSystem
        const Matrix4x4 & getWorldMatrix() const noexcept { return *m_world_matrix; }
        const Matrix4x4 & getLocalMatrix() const noexcept { return m_local_matrix; }
        const Matrix4x4 & getInvertedWorldMatrix() const noexcept;
        bool isHierarchy() const noexcept { return m_world_matrix != &m_local_matrix; }
        void translateWorld(float x, float y, float z) noexcept;
        void translateWorld(const Vector3D<float> &translation) noexcept;
        void translateLocal(float x, float y, float z) noexcept;
        void translateLocal(const Vector3D<float> &translation) noexcept;
        void translateLocalXAxis(float x) noexcept;
        void translateLocalYAxis(float y) noexcept;
        void translateLocalZAxis(float z) noexcept;
        void rotateLocal(const Quaternion &rotation) noexcept;
        void rotateLocalXAxis(float angle_degrees) noexcept;
        void rotateLocalYAxis(float angle_degrees) noexcept;
        void rotateLocalZAxis(float angle_degrees) noexcept;
        void rotateAround(const Quaternion &rotation, const Vector3D<float> &position) noexcept;
        void rotateAroundOrigin(const Quaternion &rotation) noexcept;
        void rotateAroundSelf(const Quaternion &rotation) noexcept;
        void scale(float factor) noexcept;
        void scale(float x, float y, float z) noexcept;
        void scale(const Vector3D<float> &scale) noexcept;
        void setTranslation(float x, float y, float z) noexcept;
        void setTranslation(const Vector3D<float> &position) noexcept;
        void setRotation(const Quaternion &rotation) noexcept;
        void setRotation(float angle_degrees, float x, float y, float z) noexcept;
        void setRotation(float angle_degrees, const Vector3D<float> &axis) noexcept;
        void setScale(float x, float y, float z) noexcept;
        void setScale(const Vector3D<float> &scale) noexcept;
        void setScale(float factor) noexcept;
        void setTRS(const Vector3D<float> &translation, const Quaternion &rotation, const Vector3D<float> &scale) noexcept;
        Vector3D<float> getXAxis() const noexcept;
        Vector3D<float> getYAxis() const noexcept;
        Vector3D<float> getZAxis() const noexcept;
        Vector3D<float> getTranslation() const noexcept;
        Quaternion getRotation() const noexcept;
        Vector3D<float> getScale() const noexcept;
    private:
        void markDirty() const noexcept { m_is_inverted_dirty = true; }
    private:
        Matrix4x4 m_local_matrix;
        const Matrix4x4 * m_world_matrix = &m_local_matrix;
        mutable Matrix4x4 m_inverted_world_matrix;
        mutable bool m_is_inverted_dirty = false;
    };

    class HierarchicalTransform
    {
    public:
        using ChildrenList = std::vector<HierarchicalTransform *>;
        bool setParent(HierarchicalTransform * parent);
        HierarchicalTransform * getParent() const { return m_parent; }
        const HierarchicalTransform * getRoot() const;
        const ChildrenList & getChildren() const noexcept { return m_children; }
        void markDirty() { m_dirty = true; }
        void markClean() { m_dirty = false; }
        bool isDirty() const noexcept { return m_dirty; }
        void setWorldMatrix(const Matrix4x4 & world_matrix) { m_world_matrix = world_matrix; }
        uint32_t getLevel() const noexcept { return m_level; }
        const Matrix4x4 & getWorldMatrix() const noexcept { return m_world_matrix; }
    private:
        bool addChild(HierarchicalTransform * child);
        void removeChild(HierarchicalTransform * child);
    private:
        Matrix4x4 m_world_matrix;
        ChildrenList m_children;
        HierarchicalTransform * m_parent = nullptr;
        uint16_t m_level = 0;
        bool m_dirty = true;
    };
}