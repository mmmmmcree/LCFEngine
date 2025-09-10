#pragma once

#include "glm.h"
#include <string>
#include <format>

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMQuaternion : public glm::qua<T, qualifier>
    {
        using Self = GLMQuaternion<T, qualifier>;
        using Base = glm::qua<T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
        using Mat3 = GLMMatrix3x3<T, qualifier>;
        using Mat4 = GLMMatrix4x4<T, qualifier>;
    public:
        static GLMQuaternion fromAxisAndAngle(const Vec3 &axis, float angle_deg)
        {
            return glm::angleAxis(glm::radians(angle_deg), glm::normalize(axis));
        }
        static GLMQuaternion fromAxisAndAngle(float x, float y, float z, float angle_deg)
        {
            return fromAxisAndAngle(glm::vec3(x, y, z), angle_deg);
        }
        static GLMQuaternion slerp(const GLMQuaternion &lhs, const GLMQuaternion &rhs, float t)
        {
            return glm::slerp(lhs, rhs, t);
        }
        static GLMQuaternion nlerp(const GLMQuaternion &lhs, const GLMQuaternion &rhs, float t)
        {
            if (t <= 0.0f) { return lhs; }
            else if (t >= 1.0f) { return rhs; }
            float dot = glm::dot(static_cast<glm::quat>(lhs), static_cast<glm::quat>(rhs));
            GLMQuaternion rhs2 = dot >= 0.0f ? rhs : GLMQuaternion(-rhs);
            return glm::normalize((lhs * (1.0f - t) + rhs2 * t));
        }
        GLMQuaternion() = default;
        GLMQuaternion(float scalar, float x, float y, float z) : Base(scalar, x, y, z) {}
        GLMQuaternion(const GLMQuaternion &quaternion) : Base(quaternion) {}
        GLMQuaternion(const Base & quaternion) : Base(quaternion) {}
        Self & operator=(const Base & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(const Self & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(Base && vec) noexcept { return Base::operator=(std::move(vec)); }
        Self & operator=(Self && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        void setX(T x) noexcept { this->x = x; }
        T getX() const noexcept { return this->x; }
        void setY(T y) noexcept { this->y = y; }
        T getY() const noexcept { return this->y; }
        void setZ(T z) noexcept { this->z = z; }
        T getZ() const noexcept { return this->z; }
        void setScalar(T w) noexcept { this->w = w; }
        T getScalar() const noexcept { return this->w; }
        bool isIdentity() const noexcept { return glm::all(glm::equal(*this, Self(1, 0, 0, 0))); }
        T length() const noexcept { return glm::length(*this); }
        T lengthSquared() const noexcept { return glm::dot(*this, *this); }
        Self conjugated() const noexcept { return glm::conjugate(*this); }
        void normalize() noexcept { glm::normalize(*this); }
        Self normalized() const noexcept { return glm::normalize(*this); }
        Self inverted() const noexcept { return glm::inverse(*this); }
        Vec3 rotatedVector(const Vec3 &vector) const noexcept
        {
            auto r = *this * Base(0.0f, vector) * glm::conjugate(*this);
            return Vec3(r.x, r.y, r.z);
        }
        void getAxisAndAngle(float *x, float *y, float *z, float *angle_deg)
        {
            auto length = this->length();
            if (glm::abs(length) > glm::epsilon<T>()) {
                *x = this->getX() / length;
                *y = this->getY() / length;
                *z = this->getZ() / length;
                *angle_deg = glm::degrees(2.0f * glm::acos(this->getScalar()));
            } else {
                *x = *y = *z = *angle_deg = 0.0f;
            }
        }
        bool isNormalized() const noexcept { return glm::all(glm::epsilonEqual(*this, glm::normalize(*this), glm::epsilon<T>())); }
        bool isNull() const noexcept { return glm::all(glm::equal(*this, Self(0, 0, 0, 0))); }
        Vec3 toVector3D() const noexcept { return Vec3(this->x, this->y, this->z); }
        Vec4 toVector4D() const noexcept { return Vec4(this->x, this->y, this->z, this->w); }
        Mat3 toMatrix3x3() const noexcept { return glm::mat3_cast(*this); }
        Mat4 toMatrix4x4() const noexcept { return glm::mat4_cast(*this); }
    };

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    std::string to_string(const GLMQuaternion<T, qualifier> &quat)
    {
        return std::format("GLMQuaternion({}, {}, {}, {})", quat.getX(), quat.getY(), quat.getZ(), quat.getScalar());
    }
}