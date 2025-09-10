#pragma once


#include "glm.h"
#include <string>
#include <format>

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMMatrix4x4 : public glm::mat<4, 4, T, qualifier>
    {
        using Self = GLMMatrix4x4<T, qualifier>;
        using Base = glm::mat<4, 4, T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
        using Quat = GLMQuaternion<T, qualifier>;
        using Mat3 = GLMMatrix3x3<T, qualifier>;
        using Mat4 = GLMMatrix4x4<T, qualifier>;
    public:
        GLMMatrix4x4() : Base(static_cast<T>(1)) {}
        GLMMatrix4x4(const Base &mat) : Base(mat) {}
        GLMMatrix4x4(Base &&mat) : Base(std::move(mat)) {}
        GLMMatrix4x4(const GLMMatrix4x4 &mat) = default;
        GLMMatrix4x4(GLMMatrix4x4 &&mat) = default;
        GLMMatrix4x4(float m11, float m12, float m13, float m14,
            float m21, float m22, float m23, float m24,
            float m31, float m32, float m33, float m34,
            float m41, float m42, float m43, float m44)
        {
            this->operator[](0) = Vec4(m11, m21, m31, m41);
            this->operator[](1) = Vec4(m12, m22, m32, m42);
            this->operator[](2) = Vec4(m13, m23, m33, m43);
            this->operator[](3) = Vec4(m14, m24, m34, m44);
        }
        Self & operator=(const Self &mat) noexcept { memcpy(this, &mat, sizeof(GLMMatrix4x4)); return *this; }
        Self & operator=(const Base &mat) noexcept { memcpy(this, &mat, sizeof(GLMMatrix4x4)); return *this; }
        Self operator*(const Self & mat) const noexcept { return static_cast<Base>(*this) * static_cast<Base>(mat); }
        Self & operator*=(const Self & mat) noexcept { return *this = *this * mat; }
        void setToIdentity() noexcept { *this = glm::identity<Base>(); }
        void setTranslation(const Vec3 &translation) noexcept { memcpy(&this->operator[](3), &translation, sizeof(Vec3)); }
        void setTranslation(T x, T y, T z) noexcept { this->setTranslation(Vec3(x, y, z)); }
        Vec3 getTranslation() const noexcept { return Vec3(this->operator[](3)); }
        void setRotation(const Quat &rotation) noexcept
        {
            auto rotation_mat = rotation.toMatrix3x3();
            for (int i = 0; i < 3; ++i) {
                memcpy(&this->operator[](i), &rotation_mat[i], sizeof(Vec3));
            }
        }
        void setRotation(T angle_deg, const Vec3 &axis) noexcept { this->setRotation(Quat::fromAxisAndAngle(axis, angle_deg)); }
        void setRotation(T angle_deg, T x, T y, T z) noexcept { this->setRotation(angle_deg, Vec3(x, y, z)); }
        Quat getRotation() const noexcept
        {
            Mat3 mat = *this;
            for (int i = 0; i < 3; ++i) {
                mat[i] /= glm::length(mat[i]);
            }
            return glm::quat_cast(mat);
        }
        void setScale(const Vec3 &scale) noexcept
        {
            auto s = glm::max(scale, glm::epsilon<T>());
            glm::vec3 original_scale(glm::length(this->operator[](0)), glm::length(this->operator[](1)), glm::length(this->operator[](2)));
            this->operator[](0) *= s.x / original_scale.x;
            this->operator[](1) *= s.y / original_scale.y;
            this->operator[](2) *= s.z / original_scale.z;
        }
        void setScale(T x, T y, T z) noexcept { this->setScale(Vec3(x, y, z)); }
        Vec3 getScale() const noexcept { return Vec3(glm::length(this->operator[](0)), glm::length(this->operator[](1)), glm::length(this->operator[](2))); }
        void setTRS(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) noexcept
        {
            this->setTranslation(translation);
            this->setRotation(rotation);
            this->setScale(scale);
        }
        // 右乘变换矩阵
        void translateLocal(const Vec3 &translation) noexcept { *this = glm::translate(*this, translation); }
        void translateLocal(float x, float y, float z) noexcept { this->translateLocal(Vec3(x, y, z)); }
        void translateLocalX(float x) noexcept { this->translateLocal(Vec3(x, static_cast<T>(0), static_cast<T>(0))); }
        void translateLocalY(float y) noexcept { this->translateLocal(Vec3(static_cast<T>(0), y, static_cast<T>(0))); }
        void translateLocalZ(float z) noexcept { this->translateLocal(Vec3(static_cast<T>(0), static_cast<T>(0), z)); }
        // 左乘变换矩阵
        void translateWorld(const Vec3 &translation) noexcept { this->operator[](3) += translation.toVector4D(); }
        void translateWorld(float x, float y, float z) noexcept { this->translateWorld(Vec3(x, y, z)); }
        void translateWorldX(float x) noexcept { this->translateWorld(Vec3(x, static_cast<T>(0), static_cast<T>(0))); }
        void translateWorldY(float y) noexcept { this->translateWorld(Vec3(static_cast<T>(0), y, static_cast<T>(0))); }
        void translateWorldZ(float z) noexcept { this->translateWorld(Vec3(static_cast<T>(0), static_cast<T>(0), z)); }
        // 右乘变换矩阵
        void rotateLocal(const Quat &rotation) noexcept { this->operator*=(static_cast<Base>(rotation.toMatrix4x4())); }
        void rotateLocal(T angle_deg, const Vec3 &axis) noexcept { *this = glm::rotate(*this, glm::radians(angle_deg), axis); }
        void rotateLocal(T angle_deg, T x, T y, T z) noexcept { this->rotateLocal(angle_deg, Vec3(x, y, z)); }
        // 左乘变换矩阵
        void rotateAround(const Quat &rotation, const Vec3 &position) noexcept
        {
            Mat4 mat = rotation.toMatrix4x4();
            Vec3 translation = position + rotation.rotatedVector(-position);
            memcpy(&mat[3], &translation, sizeof(Vec3));
            *this = mat * (*this);
        }
        void rotateAroundOrigin(const Quat&rotation) noexcept { *this = rotation.toMatrix4x4() * (*this); }
        void rotateAroundSelf(const Quat &rotation) noexcept { this->rotateAround(rotation, this->getTranslation()); }
        // 右乘变换矩阵
        void scale(const Vec3 &scale) noexcept { *this = glm::scale(*this, scale); }
        void scale(T x, T y, T z) noexcept { this->scale(Vec3(x, y, z)); }
        void scale(T factor) noexcept { this->scale(Vec3(factor, factor, factor)); }
        void inverse() noexcept { *this = glm::inverse(*this); }
        Self inverted() const noexcept { return glm::inverse(*this); }
        void transpose() noexcept { *this = glm::transpose(*this); }
        Self transposed() const noexcept { return glm::transpose(*this); }
        void lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) noexcept { *this = glm::lookAt(eye, center, up); }
        void ortho(T left, T right, T bottom, T top, T near_plane, T far_plane) noexcept { *this = glm::ortho(left, right, bottom, top, near_plane, far_plane); }
        void perspective(T fovy_deg, T aspect, T near_plane, T far_plane) noexcept { *this = glm::perspective(glm::radians(fovy_deg), aspect, near_plane, far_plane); }
        T * getData() noexcept { return glm::value_ptr(static_cast<Base &>(*this)); }
        const T * getData() const noexcept { return glm::value_ptr(static_cast<const Base &>(*this)); }
        const T * getConstData() const noexcept { return glm::value_ptr(static_cast<const Base &>(*this)); }
        Mat3 toMatrix3x3() const noexcept { return Mat3(*this); }
        void setColumn(int index, const Vec4 &column) noexcept { this->operator[](index) = column; }
        Vec4 column(int index) const noexcept { return this->operator[](index); }
        void setRow(int index, const Vec4 &row) noexcept
        {
            for (int i = 0; i < 4; ++i) {
                this->operator[](i)[index] = row[i];
            }
        }
        Vec4 row(int index) const noexcept { return Vec4(this->operator[](0)[index], this->operator[](1)[index], this->operator[](2)[index], this->operator[](3)[index]); }
    };

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    std::string to_string(const GLMMatrix4x4<T, qualifier> &mat)
    {
        return std::format("GLMTransformMatrix({:.4f}, {:.4f}, {:.4f}, {:.4f}\n, {:.4f}, {:.4f}, {:.4f}, {:.4f}\n, {:.4f}, {:.4f}, {:.4f}, {:.4f}\n, {:.4f}, {:.4f})", 
            mat[0][0], mat[1][0], mat[2][0], mat[3][0],
            mat[0][1], mat[1][1], mat[2][1], mat[3][1],
            mat[0][2], mat[1][2], mat[2][2], mat[3][2],
            mat[0][3], mat[1][3], mat[2][3], mat[3][3]
        );       
    }
}