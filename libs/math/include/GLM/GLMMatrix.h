#pragma once

#include "glm.h"

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMMatrix4x4
    {
        using Self = GLMMatrix4x4<T, qualifier>;
        using GlmType = glm::mat<4, 4, T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
        using Quat = GLMQuaternion<T, qualifier>;
        using Mat3 = GLMMatrix3x3<T, qualifier>;
    public:
        using value_type = T;
    public:
        GLMMatrix4x4() noexcept : m_data(static_cast<T>(1)) {}
        GLMMatrix4x4(const GlmType &mat) noexcept : m_data(mat) {}
        GLMMatrix4x4(const Self &) = default;
        GLMMatrix4x4(Self &&) = default;
        GLMMatrix4x4(T m11, T m12, T m13, T m14,
            T m21, T m22, T m23, T m24,
            T m31, T m32, T m33, T m34,
            T m41, T m42, T m43, T m44);
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
        Self & operator=(const GlmType &mat) noexcept { m_data = mat; return *this; }
    public:
        operator const GlmType&() const noexcept { return m_data; }
        operator GlmType&() noexcept { return m_data; }
    public:
        Self operator*(const Self & mat) const noexcept { return Self(m_data * mat.m_data); }
        Vec4 operator*(const Vec4 & vec) const noexcept { return Vec4(m_data * static_cast<const glm::vec<4, T, qualifier>&>(vec)); }
        Self & operator*=(const Self & mat) noexcept { m_data *= mat.m_data; return *this; }
    public:
        void setToIdentity() noexcept { m_data = glm::identity<GlmType>(); }
        void setTranslation(const Vec3 &translation) noexcept { memcpy(&m_data[3], &translation, sizeof(Vec3)); }
        void setTranslation(T x, T y, T z) noexcept { setTranslation(Vec3(x, y, z)); }
        Vec3 getTranslation() const noexcept { return Vec3(m_data[3]); }
        void setRotation(const Quat &rotation) noexcept;
        void setRotation(T angle_deg, const Vec3 &axis) noexcept { setRotation(Quat::fromAxisAndAngle(axis, angle_deg)); }
        void setRotation(T angle_deg, T x, T y, T z) noexcept { setRotation(angle_deg, Vec3(x, y, z)); }
        Quat getRotation() const noexcept;
        void setScale(const Vec3 &scale) noexcept;
        void setScale(T x, T y, T z) noexcept { setScale(Vec3(x, y, z)); }
        Vec3 getScale() const noexcept { return Vec3(glm::length(m_data[0]), glm::length(m_data[1]), glm::length(m_data[2])); }
        void setTRS(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) noexcept;
        void translateLocal(const Vec3 &translation) noexcept { m_data = glm::translate(m_data, static_cast<const glm::vec<3, T, qualifier>&>(translation)); }
        void translateLocal(T x, T y, T z) noexcept { translateLocal(Vec3(x, y, z)); }
        void translateLocalX(T x) noexcept { translateLocal(Vec3(x, T(0), T(0))); }
        void translateLocalY(T y) noexcept { translateLocal(Vec3(T(0), y, T(0))); }
        void translateLocalZ(T z) noexcept { translateLocal(Vec3(T(0), T(0), z)); }
        void translateWorld(const Vec3 &translation) noexcept { m_data[3] += glm::vec<4, T, qualifier>(static_cast<const glm::vec<3, T, qualifier>&>(translation), T(0)); }
        void translateWorld(T x, T y, T z) noexcept { translateWorld(Vec3(x, y, z)); }
        void translateWorldX(T x) noexcept { translateWorld(Vec3(x, T(0), T(0))); }
        void translateWorldY(T y) noexcept { translateWorld(Vec3(T(0), y, T(0))); }
        void translateWorldZ(T z) noexcept { translateWorld(Vec3(T(0), T(0), z)); }
        void rotateLocal(const Quat &rotation) noexcept { m_data *= static_cast<const GlmType&>(rotation.toMatrix4x4()); }
        void rotateLocal(T angle_deg, const Vec3 &axis) noexcept { m_data = glm::rotate(m_data, glm::radians(angle_deg), static_cast<const glm::vec<3, T, qualifier>&>(axis)); }
        void rotateLocal(T angle_deg, T x, T y, T z) noexcept { rotateLocal(angle_deg, Vec3(x, y, z)); }
        void rotateAround(const Quat &rotation, const Vec3 &position) noexcept;
        void rotateAroundOrigin(const Quat &rotation) noexcept { m_data = static_cast<const GlmType&>(rotation.toMatrix4x4()) * m_data; }
        void rotateAroundSelf(const Quat &rotation) noexcept { rotateAround(rotation, getTranslation()); }
        void scale(const Vec3 &s) noexcept { m_data = glm::scale(m_data, static_cast<const glm::vec<3, T, qualifier>&>(s)); }
        void scale(T x, T y, T z) noexcept { scale(Vec3(x, y, z)); }
        void scale(T factor) noexcept { scale(Vec3(factor, factor, factor)); }
        void inverse() noexcept { m_data = glm::inverse(m_data); }
        Self inverted() const noexcept { return Self(glm::inverse(m_data)); }
        void transpose() noexcept { m_data = glm::transpose(m_data); }
        Self transposed() const noexcept { return Self(glm::transpose(m_data)); }
        void lookAtLH(const Vec3 &eye, const Vec3 &center, const Vec3 &up) noexcept { m_data = glm::lookAtLH(static_cast<const glm::vec<3, T, qualifier>&>(eye), static_cast<const glm::vec<3, T, qualifier>&>(center), static_cast<const glm::vec<3, T, qualifier>&>(up)); }
        void lookAtRH(const Vec3 &eye, const Vec3 &center, const Vec3 &up) noexcept { m_data = glm::lookAtRH(static_cast<const glm::vec<3, T, qualifier>&>(eye), static_cast<const glm::vec<3, T, qualifier>&>(center), static_cast<const glm::vec<3, T, qualifier>&>(up)); }
        void orthoLH(T left, T right, T bottom, T top, T near_plane, T far_plane) noexcept { m_data = glm::orthoLH(left, right, bottom, top, near_plane, far_plane); }
        void orthoRH(T left, T right, T bottom, T top, T near_plane, T far_plane) noexcept { m_data = glm::orthoRH(left, right, bottom, top, near_plane, far_plane); }
        void orthoZO(T left, T right, T bottom, T top, T near_plane, T far_plane) noexcept { m_data = glm::orthoZO(left, right, bottom, top, near_plane, far_plane); }
        void orthoNO(T left, T right, T bottom, T top, T near_plane, T far_plane) noexcept { m_data = glm::orthoNO(left, right, bottom, top, near_plane, far_plane); }
        void perspectiveLH_ZO(T fovy_deg, T aspect, T near_plane, T far_plane) noexcept { m_data = glm::perspectiveLH_ZO(glm::radians(fovy_deg), aspect, near_plane, far_plane); }
        void perspectiveLH_NO(T fovy_deg, T aspect, T near_plane, T far_plane) noexcept { m_data = glm::perspectiveLH_NO(glm::radians(fovy_deg), aspect, near_plane, far_plane); }
        void perspectiveRH_ZO(T fovy_deg, T aspect, T near_plane, T far_plane) noexcept { m_data = glm::perspectiveRH_ZO(glm::radians(fovy_deg), aspect, near_plane, far_plane); }
        void perspectiveRH_NO(T fovy_deg, T aspect, T near_plane, T far_plane) noexcept { m_data = glm::perspectiveRH_NO(glm::radians(fovy_deg), aspect, near_plane, far_plane); }
        T * getData() noexcept { return glm::value_ptr(m_data); }
        const T * getData() const noexcept { return glm::value_ptr(m_data); }
        const T * getConstData() const noexcept { return glm::value_ptr(m_data); }
        Mat3 toMatrix3x3() const noexcept { return Mat3(m_data); }
        void setColumn(int index, const Vec4 &column) noexcept { m_data[index] = static_cast<const glm::vec<4, T, qualifier>&>(column); }
        Vec4 getColumn(int index) const noexcept { return Vec4(m_data[index]); }
        void setRow(int index, const Vec4 &row) noexcept;
        Vec4 getRow(int index) const noexcept;
    public:
        GlmType m_data;
    };
}

template <lcf::number_c T, glm::qualifier qualifier>
inline lcf::GLMMatrix4x4<T, qualifier>::GLMMatrix4x4(
    T m11, T m12, T m13, T m14,
    T m21, T m22, T m23, T m24,
    T m31, T m32, T m33, T m34,
    T m41, T m42, T m43, T m44)
{
    m_data[0] = glm::vec<4, T, qualifier>(m11, m21, m31, m41);
    m_data[1] = glm::vec<4, T, qualifier>(m12, m22, m32, m42);
    m_data[2] = glm::vec<4, T, qualifier>(m13, m23, m33, m43);
    m_data[3] = glm::vec<4, T, qualifier>(m14, m24, m34, m44);
}

template <lcf::number_c T, glm::qualifier qualifier>
inline void lcf::GLMMatrix4x4<T, qualifier>::setRotation(const Quat &rotation) noexcept
{
    auto rotation_mat = rotation.toMatrix3x3();
    for (int i = 0; i < 3; ++i) {
        memcpy(&m_data[i], &rotation_mat[i], sizeof(Vec3));
    }
}

template <lcf::number_c T, glm::qualifier qualifier>
inline lcf::GLMMatrix4x4<T, qualifier>::Quat lcf::GLMMatrix4x4<T, qualifier>::getRotation() const noexcept
{
    Mat3 mat(m_data);
    for (int i = 0; i < 3; ++i) {
        mat[i] /= glm::length(mat[i]);
    }
    return glm::quat_cast(mat);
}

template <lcf::number_c T, glm::qualifier qualifier>
inline void lcf::GLMMatrix4x4<T, qualifier>::setScale(const Vec3 &scale) noexcept
{
    auto s = glm::max(static_cast<const glm::vec<3, T, qualifier>&>(scale), glm::vec<3, T, qualifier>(glm::epsilon<T>()));
    auto original_scale = getScale();
    m_data[0] *= s.x / original_scale.getX();
    m_data[1] *= s.y / original_scale.getY();
    m_data[2] *= s.z / original_scale.getZ();
}

template <lcf::number_c T, glm::qualifier qualifier>
inline void lcf::GLMMatrix4x4<T, qualifier>::setTRS(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) noexcept
{
    setTranslation(translation);
    setRotation(rotation);
    setScale(scale);
}

template <lcf::number_c T, glm::qualifier qualifier>
inline void lcf::GLMMatrix4x4<T, qualifier>::rotateAround(const Quat &rotation, const Vec3 &position) noexcept
{
    Self mat = rotation.toMatrix4x4();
    Vec3 translation = position + rotation.rotatedVector(-position);
    memcpy(&mat.m_data[3], &translation, sizeof(Vec3));
    m_data = mat.m_data * m_data;
}

template <lcf::number_c T, glm::qualifier qualifier>
inline void lcf::GLMMatrix4x4<T, qualifier>::setRow(int index, const Vec4 &row) noexcept
{
    for (int i = 0; i < 4; ++i) {
        m_data[i][index] = row[i];
    }
}

template <lcf::number_c T, glm::qualifier qualifier>
inline lcf::GLMMatrix4x4<T, qualifier>::Vec4 lcf::GLMMatrix4x4<T, qualifier>::getRow(int index) const noexcept
{
    return Vec4(m_data[0][index], m_data[1][index], m_data[2][index], m_data[3][index]);
}
