#pragma once

#include "Matrix.h"
#include "Vector.h"
#include "Quaternion.h"
#include "PointerDefs.h"

namespace lcf {
    class Transform : public STDPointerDefs<Transform>
    {
    public:
        Transform() = default;
        Transform(const Transform &other);
        Transform &operator=(const Transform &other);
        void setLocalMatrix(const Matrix4x4 & matrix);
        void setWorldMatrix(const Matrix4x4 * world_matrix); // controlled by TransformSystem
        const Matrix4x4 & getWorldMatrix() const { return *m_world_matrix; }
        const Matrix4x4 & getLocalMatrix() const { return m_local_matrix; }
        const Matrix4x4 & getInvertedWorldMatrix() const;
        bool isHierarchy() const { return m_world_matrix != &m_local_matrix; }
        void translateWorld(float x, float y, float z);
        void translateWorld(const Vector3D &translation);
        void translateLocal(float x, float y, float z);
        void translateLocal(const Vector3D &translation);
        void translateLocalXAxis(float x);
        void translateLocalYAxis(float y);
        void translateLocalZAxis(float z);
        void rotateLocal(const Quaternion &rotation);
        void rotateLocalXAxis(float angle_degrees);
        void rotateLocalYAxis(float angle_degrees);
        void rotateLocalZAxis(float angle_degrees);
        void rotateAround(const Quaternion &rotation, const Vector3D &position);
        void rotateAroundOrigin(const Quaternion &rotation);
        void rotateAroundSelf(const Quaternion &rotation);
        void scale(float factor);
        void scale(float x, float y, float z);
        void scale(const Vector3D &scale);
        void setTranslation(float x, float y, float z);
        void setTranslation(const Vector3D &position);
        void setRotation(const Quaternion &rotation);
        void setRotation(float angle_degrees, float x, float y, float z);
        void setRotation(float angle_degrees, const Vector3D &axis);
        void setScale(float x, float y, float z);
        void setScale(const Vector3D &scale);
        void setScale(float factor);
        void setTRS(const Vector3D &translation, const Quaternion &rotation, const Vector3D &scale);
        Vector3D getXAxis() const;
        Vector3D getYAxis() const;
        Vector3D getZAxis() const;
        Vector3D getTranslation() const;
        Quaternion getRotation() const;
        Vector3D getScale() const;
    private:
        void markDirty() const { m_is_inverted_dirty = true; }
    private:
        Matrix4x4 m_local_matrix;
        const Matrix4x4 * m_world_matrix = &m_local_matrix;
        mutable Matrix4x4 m_inverted_world_matrix;
        mutable bool m_is_inverted_dirty = false;
    };
}