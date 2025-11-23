#pragma once

#include "glm.h"
#include <string>
#include <format>


namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMVector2D : public glm::vec<2, T, qualifier>
    {
        using Self = GLMVector2D<T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
    public:
        using Base = glm::vec<2, T, qualifier>;
        using value_type = T;
        constexpr GLMVector2D() noexcept : Base(static_cast<T>(0), static_cast<T>(0)) {}
        constexpr GLMVector2D(T x, T y) noexcept : Base(x, y) {}
        GLMVector2D(const Vec3::Base &vec) : Base(vec.x, vec.y) {}
        GLMVector2D(const Vec4::Base &vec) : Base(vec.x, vec.y) {}
        GLMVector2D(const Base &vec) { memcpy(this, &vec, sizeof(Base)); }
        GLMVector2D(const Self & other) : Base(other) {}
        GLMVector2D(Self && other) : Base(std::move(other)) {}
        GLMVector2D(Base && other) : Base(std::move(other)) {}
        Self & operator=(const Base & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(const Self & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(Base && vec) noexcept { return Base::operator=(std::move(vec)); }
        Self & operator=(Self && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        Self operator+(const Self & vec) const noexcept{ return static_cast<Base>(*this) + static_cast<Base>(vec); }
        Self operator-(const Self & vec) const noexcept { return static_cast<Base>(*this) - static_cast<Base>(vec); }
        Self operator*(const Self & vec) const noexcept { return static_cast<Base>(*this) * static_cast<Base>(vec); }
        Self operator/(const Self & vec) const noexcept { return static_cast<Base>(*this) / static_cast<Base>(vec); }
        Self operator*(T scalar) const noexcept { return static_cast<Base>(*this) * scalar; }
        Self operator/(T scalar) const noexcept { return static_cast<Base>(*this) / scalar; }
        Self & operator+=(const Self & vec) noexcept { *this = *this + vec; return *this; }
        Self & operator-=(const Self & vec) noexcept { *this = *this - vec; return *this; }
        Self & operator*=(const Self & vec) noexcept { *this = *this * vec; return *this; }
        Self & operator/=(const Self & vec) noexcept { *this = *this / vec; return *this; }
        Self & operator*=(T scalar) noexcept { *this = *this * scalar; return *this; }
        Self & operator/=(T scalar) noexcept { *this = *this / scalar; return *this; }
        void setX(float x) noexcept { this->x = x; }
        T getX() const noexcept { return this->x; }
        void setY(float y) noexcept { this->y = y; }
        T getY() const noexcept { return this->y; }
        Vec3 toVector3D() const noexcept { return Vec3(*this); }
        Vec4 toVector4D() const noexcept { return Vec4(*this); }
    };

    template <number_c T, glm::qualifier qualifier>
    class GLMVector3D : public glm::vec<3, T, qualifier>
    {
        using Self = GLMVector3D<T, qualifier>;
        using Vec2 = GLMVector2D<T, qualifier>;
        using Vec4 = GLMVector4D<T, qualifier>;
    public:
        using Base = glm::vec<3, T, qualifier>;
        static Self crossProduct(const GLMVector3D &lhs, const GLMVector3D &rhs)
        {
            return glm::cross(lhs, rhs);
        }
        static float dotProduct(const GLMVector3D &lhs, const GLMVector3D &rhs)
        {
            return glm::dot(lhs, rhs);
        }
    public:
        using value_type = T;
        constexpr GLMVector3D() noexcept : Base(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)) {}
        constexpr GLMVector3D(T x, T y, T z) noexcept : Base(x, y, z) {}
        GLMVector3D(const Vec2::Base &vec, T z = static_cast<T>(0)) : Base(vec.x, vec.y, z) {}
        GLMVector3D(const Vec4::Base &vec) : Base(vec.x, vec.y, vec.z) {}
        GLMVector3D(const Base &vec) { memcpy(this, &vec, sizeof(Base)); }
        GLMVector3D(const Self & other) : Base(other) {}
        GLMVector3D(Self && other) : Base(std::move(other)) {}
        GLMVector3D(Base && other) : Base(std::move(other)) {}
        Self & operator=(const Base & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(const Self & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(Base && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        Self & operator=(Self && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        Self operator+(const Self & vec) const noexcept{ return static_cast<Base>(*this) + static_cast<Base>(vec); }
        Self operator-(const Self & vec) const noexcept { return static_cast<Base>(*this) - static_cast<Base>(vec); }
        Self operator*(const Self & vec) const noexcept { return static_cast<Base>(*this) * static_cast<Base>(vec); }
        Self operator/(const Self & vec) const noexcept { return static_cast<Base>(*this) / static_cast<Base>(vec); }
        Self operator*(T scalar) const noexcept { return static_cast<Base>(*this) * scalar; }
        Self operator/(T scalar) const noexcept { return static_cast<Base>(*this) / scalar; }
        Self & operator+=(const Self & vec) noexcept { *this = *this + vec; return *this; }
        Self & operator-=(const Self & vec) noexcept { *this = *this - vec; return *this; }
        Self & operator*=(const Self & vec) noexcept { *this = *this * vec; return *this; }
        Self & operator/=(const Self & vec) noexcept { *this = *this / vec; return *this; }
        Self & operator*=(T scalar) noexcept { *this = *this * scalar; return *this; }
        Self & operator/=(T scalar) noexcept { *this = *this / scalar; return *this; }
        void setX(float x) noexcept { this->x = x; }
        T getX() const noexcept { return this->x; }
        void setY(float y) noexcept { this->y = y; }
        T getY() const noexcept { return this->y; }
        void setZ(float z) noexcept { this->z = z; }
        T getZ() const noexcept { return this->z; }
        bool isNull() const noexcept { return glm::dot(static_cast<Base>(*this), static_cast<Base>(*this)) <= glm::epsilon<T>(); }
        void normalize() noexcept { *this = glm::normalize(*this); }
        GLMVector3D normalized() const noexcept { return glm::normalize(*this); }
        Vec2 toVector2D() const noexcept { return Vec2(*this); }
        Vec4 toVector4D() const noexcept { return Vec4(*this); }
    };

    template <number_c T, glm::qualifier qualifier>
    class GLMVector4D : public glm::vec<4, T, qualifier>
    {
        using Self = GLMVector4D<T, qualifier>;
        using Vec2 = GLMVector2D<T, qualifier>;
        using Vec3 = GLMVector3D<T, qualifier>;
    public:
        using Base = glm::vec<4, T, qualifier>;
        using value_type = T;
        constexpr GLMVector4D() noexcept : Base(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)) {}
        constexpr GLMVector4D(T x, T y, T z, T w) noexcept : Base(x, y, z, w) {}
        GLMVector4D(const Vec2::Base &vec, T z = static_cast<T>(0), T w = static_cast<T>(1)) : Base(vec.x, vec.y, z, w) {}
        GLMVector4D(const Vec3::Base &vec, T w = static_cast<T>(1)) : Base(vec.x, vec.y, vec.z, w) {}
        GLMVector4D(const Base &vec) noexcept { memcpy(this, &vec, sizeof(Base)); }
        GLMVector4D(Base && vec) noexcept : Base(std::move(vec)) {}
        GLMVector4D(const Self & other) : Base(other) {}
        GLMVector4D(Self && other) : Base(std::move(other)) {}
        Self & operator=(const Base & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(const Self & vec) noexcept { memcpy(this, &vec, sizeof(Base)); return *this; }
        Self & operator=(Base && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        Self & operator=(Self && vec) noexcept { Base::operator=(std::move(vec)); return *this; }
        Self operator+(const Self & vec) const noexcept{ return static_cast<Base>(*this) + static_cast<Base>(vec); }
        Self operator-(const Self & vec) const noexcept { return static_cast<Base>(*this) - static_cast<Base>(vec); }
        Self operator*(const Self & vec) const noexcept { return static_cast<Base>(*this) * static_cast<Base>(vec); }
        Self operator/(const Self & vec) const noexcept { return static_cast<Base>(*this) / static_cast<Base>(vec); }
        Self operator*(T scalar) const noexcept { return static_cast<Base>(*this) * scalar; }
        Self operator/(T scalar) const noexcept { return static_cast<Base>(*this) / scalar; }
        Self & operator+=(const Self & vec) noexcept { *this = *this + vec; return *this; }
        Self & operator-=(const Self & vec) noexcept { *this = *this - vec; return *this; }
        Self & operator*=(const Self & vec) noexcept { *this = *this * vec; return *this; }
        Self & operator/=(const Self & vec) noexcept { *this = *this / vec; return *this; }
        Self & operator*=(T scalar) noexcept { *this = *this * scalar; return *this; }
        Self & operator/=(T scalar) noexcept { *this = *this / scalar; return *this; }
        void setX(float x) noexcept { this->x = x; }
        T getX() const noexcept { return this->x; }
        void setY(float y) noexcept { this->y = y; }
        T getY() const noexcept { return this->y; }
        void setZ(float z) noexcept { this->z = z; }
        T getZ() const noexcept { return this->z; }
        void setW(float w) noexcept { this->w = w; }
        T getW() const noexcept { return this->w; }
        Vec2 toVector2D() const noexcept { return Vec2(*this); }
        Vec3 toVector3D() const noexcept { return Vec3(*this); }       
        void normalize() noexcept { *this = glm::normalize(*this); }
        GLMVector4D normalized() const noexcept { return glm::normalize(*this); }
        bool isNull() const noexcept { return glm::dot(*this, *this) <= glm::epsilon<T>(); }
    };

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    std::string to_string(const GLMVector2D<T, qualifier> &vec)
    {
        return std::format("GLMVector2D({}, {})", vec.getX(), vec.getY());
    }

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    std::string to_string(const GLMVector3D<T, qualifier> &vec)
    {
        return std::format("GLMVector3D({}, {}, {})", vec.getX(), vec.getY(), vec.getZ());
    }

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    std::string to_string(const GLMVector4D<T, qualifier> &vec)
    {
        return std::format("GLMVector4D({}, {}, {}, {})", vec.getX(), vec.getY(), vec.getZ(), vec.getW());
    }
}

namespace std {
    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector2D<T, qualifier>> : integral_constant<size_t, 2> {};

    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector2D<T, qualifier>> { using type = T; };
    
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    constexpr auto get(lcf::GLMVector2D<T, qualifier>&& vec) noexcept
    {
        return std::forward<decltype(vec)>(vec)[I];
    }

    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector3D<T, qualifier>> : integral_constant<size_t, 3> {};

    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector3D<T, qualifier>> { using type = T; };
    
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    constexpr auto get(lcf::GLMVector3D<T, qualifier>&& vec) noexcept
    {
        return std::forward<decltype(vec)>(vec)[I];
    }

    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector4D<T, qualifier>> : integral_constant<size_t, 4> {};

    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector4D<T, qualifier>> { using type = T; };
    
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    constexpr auto get(lcf::GLMVector4D<T, qualifier>&& vec) noexcept
    {
        return std::forward<decltype(vec)>(vec)[I];
    }
}

