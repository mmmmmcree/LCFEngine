#pragma once

#include "glm.h"

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMQuaternion
    {
        static_assert(sizeof(T) * 4 == sizeof(glm::qua<T, qualifier>));
        using Self = GLMQuaternion<T, qualifier>;
        using GlmType = glm::qua<T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
        using Mat3 = GLMMatrix3x3<T, qualifier>;
        using Mat4 = GLMMatrix4x4<T, qualifier>;
    public:
        using value_type = T;
    public:
        static Self fromAxisAndAngle(const Vec3 &axis, T angle_deg)
        {
            return Self(glm::angleAxis(glm::radians(angle_deg), glm::normalize(static_cast<const glm::vec<3, T, qualifier>&>(axis))));
        }
        static Self fromAxisAndAngle(T x, T y, T z, T angle_deg)
        {
            return fromAxisAndAngle(Vec3(x, y, z), angle_deg);
        }
        static Self slerp(const Self &lhs, const Self &rhs, T t)
        {
            return Self(glm::slerp(lhs.m_data, rhs.m_data, t));
        }
        static Self nlerp(const Self &lhs, const Self &rhs, T t)
        {
            if (t <= T(0)) { return lhs; }
            else if (t >= T(1)) { return rhs; }
            T d = glm::dot(lhs.m_data, rhs.m_data);
            GlmType rhs2 = d >= T(0) ? rhs.m_data : -rhs.m_data;
            return Self(glm::normalize(lhs.m_data * (T(1) - t) + rhs2 * t));
        }
    public:
        GLMQuaternion() noexcept : m_data(T(1), T(0), T(0), T(0)) {}
        GLMQuaternion(T w, T x, T y, T z) noexcept : m_data(w, x, y, z) {}
        GLMQuaternion(const GlmType &quat) noexcept : m_data(quat) {}
        GLMQuaternion(const Self &) = default;
        GLMQuaternion(Self &&) = default;
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
        Self & operator=(const GlmType &quat) noexcept { m_data = quat; return *this; }
    public:
        operator const GlmType&() const noexcept { return m_data; }
        operator GlmType&() noexcept { return m_data; }
    public:
        Self operator*(const Self &rhs) const noexcept { return Self(m_data * rhs.m_data); }
        Self & operator*=(const Self &rhs) noexcept { m_data *= rhs.m_data; return *this; }
    public:
        T getX() const noexcept { return m_data.x; }
        T getY() const noexcept { return m_data.y; }
        T getZ() const noexcept { return m_data.z; }
        T getScalar() const noexcept { return m_data.w; }
        void setX(T x) noexcept { m_data.x = x; }
        void setY(T y) noexcept { m_data.y = y; }
        void setZ(T z) noexcept { m_data.z = z; }
        void setScalar(T w) noexcept { m_data.w = w; }
        bool isIdentity() const noexcept { return glm::all(glm::equal(m_data, GlmType(T(1), T(0), T(0), T(0)))); }
        T length() const noexcept { return glm::length(m_data); }
        T lengthSquared() const noexcept { return glm::dot(m_data, m_data); }
        Self conjugated() const noexcept { return Self(glm::conjugate(m_data)); }
        void normalize() noexcept { m_data = glm::normalize(m_data); }
        Self normalized() const noexcept { return Self(glm::normalize(m_data)); }
        Self inverted() const noexcept { return Self(glm::inverse(m_data)); }
        Vec3 rotatedVector(const Vec3 &vector) const noexcept
        {
            auto v = static_cast<const glm::vec<3, T, qualifier>&>(vector);
            auto r = m_data * GlmType(T(0), v.x, v.y, v.z) * glm::conjugate(m_data);
            return Vec3(r.x, r.y, r.z);
        }
        void getAxisAndAngle(T *x, T *y, T *z, T *angle_deg) const noexcept
        {
            auto len = glm::length(glm::vec<3, T, qualifier>(m_data.x, m_data.y, m_data.z));
            if (len > glm::epsilon<T>()) {
                *x = m_data.x / len;
                *y = m_data.y / len;
                *z = m_data.z / len;
                *angle_deg = glm::degrees(T(2) * glm::acos(m_data.w));
            } else {
                *x = *y = *z = *angle_deg = T(0);
            }
        }
        bool isNormalized() const noexcept { return glm::abs(glm::dot(m_data, m_data) - T(1)) <= glm::epsilon<T>(); }
        bool isNull() const noexcept { return glm::dot(m_data, m_data) <= glm::epsilon<T>(); }
        Vec3 toVector3D() const noexcept { return Vec3(m_data.x, m_data.y, m_data.z); }
        Vec4 toVector4D() const noexcept { return Vec4(m_data.x, m_data.y, m_data.z, m_data.w); }
        Mat3 toMatrix3x3() const noexcept { return glm::mat3_cast(m_data); }
        Mat4 toMatrix4x4() const noexcept { return Mat4(glm::mat4_cast(m_data)); }
    private:
        GlmType m_data;
    };
}
