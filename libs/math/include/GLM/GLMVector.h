#pragma once

#include "glm.h"

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    class GLMVector2D
    {
        using Self = GLMVector2D<T, qualifier>;
        using GlmType = glm::vec<2, T, qualifier>;
    public:
        using value_type = T;
    public:
        constexpr GLMVector2D() noexcept : m_data(T(0), T(0)) {}
        constexpr GLMVector2D(T x, T y) noexcept : m_data(x, y) {}
        GLMVector2D(const glm::vec<3, T, qualifier> &vec) : m_data(vec.x, vec.y) {}
        GLMVector2D(const glm::vec<4, T, qualifier> &vec) : m_data(vec.x, vec.y) {}
        GLMVector2D(const GlmType &vec) : m_data(vec) {}
        GLMVector2D(const Self &) = default;
        GLMVector2D(Self &&) = default;
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
        Self & operator=(const GlmType &vec) noexcept { m_data = vec; return *this; }
    public:
        operator const GlmType&() const noexcept { return m_data; }
        operator GlmType&() noexcept { return m_data; }
    public:
        Self operator+(const Self & v) const noexcept { return Self(m_data + v.m_data); }
        Self operator-(const Self & v) const noexcept { return Self(m_data - v.m_data); }
        Self operator*(const Self & v) const noexcept { return Self(m_data * v.m_data); }
        Self operator/(const Self & v) const noexcept { return Self(m_data / v.m_data); }
        Self operator*(T s) const noexcept { return Self(m_data * s); }
        Self operator/(T s) const noexcept { return Self(m_data / s); }
        Self operator-() const noexcept { return Self(-m_data); }
        Self & operator+=(const Self & v) noexcept { m_data += v.m_data; return *this; }
        Self & operator-=(const Self & v) noexcept { m_data -= v.m_data; return *this; }
        Self & operator*=(const Self & v) noexcept { m_data *= v.m_data; return *this; }
        Self & operator/=(const Self & v) noexcept { m_data /= v.m_data; return *this; }
        Self & operator*=(T s) noexcept { m_data *= s; return *this; }
        Self & operator/=(T s) noexcept { m_data /= s; return *this; }
        T & operator[](size_t i) noexcept { return m_data[static_cast<int>(i)]; }
        const T & operator[](size_t i) const noexcept { return m_data[static_cast<int>(i)]; }
    public:
        T getX() const noexcept { return m_data.x; }
        T getY() const noexcept { return m_data.y; }
        void setX(T x) noexcept { m_data.x = x; }
        void setY(T y) noexcept { m_data.y = y; }
        GLMVector3D<T, qualifier> toVector3D() const noexcept;
        GLMVector4D<T, qualifier> toVector4D() const noexcept;
    private:
        GlmType m_data;
    };

    template <number_c T, glm::qualifier qualifier>
    class GLMVector3D
    {
        using Self = GLMVector3D<T, qualifier>;
        using GlmType = glm::vec<3, T, qualifier>;
    public:
        using value_type = T;
    public:
        constexpr GLMVector3D() noexcept : m_data(T(0), T(0), T(0)) {}
        constexpr GLMVector3D(T x, T y, T z) noexcept : m_data(x, y, z) {}
        GLMVector3D(const glm::vec<2, T, qualifier> &vec, T z = T(0)) : m_data(vec.x, vec.y, z) {}
        GLMVector3D(const glm::vec<4, T, qualifier> &vec) : m_data(vec.x, vec.y, vec.z) {}
        GLMVector3D(const GlmType &vec) : m_data(vec) {}
        GLMVector3D(const Self &) = default;
        GLMVector3D(Self &&) = default;
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
        Self & operator=(const GlmType &vec) noexcept { m_data = vec; return *this; }
    public:
        operator const GlmType&() const noexcept { return m_data; }
        operator GlmType&() noexcept { return m_data; }
    public:
        Self operator+(const Self & v) const noexcept { return Self(m_data + v.m_data); }
        Self operator-(const Self & v) const noexcept { return Self(m_data - v.m_data); }
        Self operator*(const Self & v) const noexcept { return Self(m_data * v.m_data); }
        Self operator/(const Self & v) const noexcept { return Self(m_data / v.m_data); }
        Self operator*(T s) const noexcept { return Self(m_data * s); }
        Self operator/(T s) const noexcept { return Self(m_data / s); }
        Self operator-() const noexcept { return Self(-m_data); }
        Self & operator+=(const Self & v) noexcept { m_data += v.m_data; return *this; }
        Self & operator-=(const Self & v) noexcept { m_data -= v.m_data; return *this; }
        Self & operator*=(const Self & v) noexcept { m_data *= v.m_data; return *this; }
        Self & operator/=(const Self & v) noexcept { m_data /= v.m_data; return *this; }
        Self & operator*=(T s) noexcept { m_data *= s; return *this; }
        Self & operator/=(T s) noexcept { m_data /= s; return *this; }
        T & operator[](size_t i) noexcept { return m_data[static_cast<int>(i)]; }
        const T & operator[](size_t i) const noexcept { return m_data[static_cast<int>(i)]; }
    public:
        T getX() const noexcept { return m_data.x; }
        T getY() const noexcept { return m_data.y; }
        T getZ() const noexcept { return m_data.z; }
        void setX(T x) noexcept { m_data.x = x; }
        void setY(T y) noexcept { m_data.y = y; }
        void setZ(T z) noexcept { m_data.z = z; }
        void fill(T value) noexcept { m_data.x = m_data.y = m_data.z = value; }
        bool isNull() const noexcept { return glm::dot(m_data, m_data) <= glm::epsilon<T>(); }
        T length() const noexcept { return glm::length(m_data); }
        T lengthSquared() const noexcept { return glm::dot(m_data, m_data); }
        void normalize() noexcept { m_data = glm::normalize(m_data); }
        Self normalized() const noexcept { return Self(glm::normalize(m_data)); }
        GLMVector2D<T, qualifier> toVector2D() const noexcept { return GLMVector2D<T, qualifier>(m_data.x, m_data.y); }
        GLMVector4D<T, qualifier> toVector4D() const noexcept;
    private:
        GlmType m_data;
    };

    template <number_c T, glm::qualifier qualifier>
    class GLMVector4D
    {
        using Self = GLMVector4D<T, qualifier>;
        using GlmType = glm::vec<4, T, qualifier>;
    public:
        using value_type = T;
    public:
        constexpr GLMVector4D() noexcept : m_data(T(0), T(0), T(0), T(0)) {}
        constexpr GLMVector4D(T x, T y, T z, T w) noexcept : m_data(x, y, z, w) {}
        GLMVector4D(const glm::vec<2, T, qualifier> &vec, T z = T(0), T w = T(1)) : m_data(vec.x, vec.y, z, w) {}
        GLMVector4D(const glm::vec<3, T, qualifier> &vec, T w = T(1)) : m_data(vec.x, vec.y, vec.z, w) {}
        GLMVector4D(const GlmType &vec) : m_data(vec) {}
        GLMVector4D(const Self &) = default;
        GLMVector4D(Self &&) = default;
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
        Self & operator=(const GlmType &vec) noexcept { m_data = vec; return *this; }
    public:
        operator const GlmType&() const noexcept { return m_data; }
        operator GlmType&() noexcept { return m_data; }
    public:
        Self operator+(const Self & v) const noexcept { return Self(m_data + v.m_data); }
        Self operator-(const Self & v) const noexcept { return Self(m_data - v.m_data); }
        Self operator*(const Self & v) const noexcept { return Self(m_data * v.m_data); }
        Self operator/(const Self & v) const noexcept { return Self(m_data / v.m_data); }
        Self operator*(T s) const noexcept { return Self(m_data * s); }
        Self operator/(T s) const noexcept { return Self(m_data / s); }
        Self operator-() const noexcept { return Self(-m_data); }
        Self & operator+=(const Self & v) noexcept { m_data += v.m_data; return *this; }
        Self & operator-=(const Self & v) noexcept { m_data -= v.m_data; return *this; }
        Self & operator*=(const Self & v) noexcept { m_data *= v.m_data; return *this; }
        Self & operator/=(const Self & v) noexcept { m_data /= v.m_data; return *this; }
        Self & operator*=(T s) noexcept { m_data *= s; return *this; }
        Self & operator/=(T s) noexcept { m_data /= s; return *this; }
        T & operator[](size_t i) noexcept { return m_data[static_cast<int>(i)]; }
        const T & operator[](size_t i) const noexcept { return m_data[static_cast<int>(i)]; }
    public:
        T getX() const noexcept { return m_data.x; }
        T getY() const noexcept { return m_data.y; }
        T getZ() const noexcept { return m_data.z; }
        T getW() const noexcept { return m_data.w; }
        void setX(T x) noexcept { m_data.x = x; }
        void setY(T y) noexcept { m_data.y = y; }
        void setZ(T z) noexcept { m_data.z = z; }
        void setW(T w) noexcept { m_data.w = w; }
        GLMVector2D<T, qualifier> toVector2D() const noexcept { return GLMVector2D<T, qualifier>(m_data.x, m_data.y); }
        GLMVector3D<T, qualifier> toVector3D() const noexcept { return GLMVector3D<T, qualifier>(m_data.x, m_data.y, m_data.z); }
        T length() const noexcept { return glm::length(m_data); }
        T lengthSquared() const noexcept { return glm::dot(m_data, m_data); }
        void normalize() noexcept { m_data = glm::normalize(m_data); }
        Self normalized() const noexcept { return Self(glm::normalize(m_data)); }
        bool isNull() const noexcept { return glm::dot(m_data, m_data) <= glm::epsilon<T>(); }
    private:
        GlmType m_data;
    };

    template <number_c T, glm::qualifier qualifier>
    GLMVector3D<T, qualifier> GLMVector2D<T, qualifier>::toVector3D() const noexcept { return GLMVector3D<T, qualifier>(m_data.x, m_data.y, T(0)); }
    template <number_c T, glm::qualifier qualifier>
    GLMVector4D<T, qualifier> GLMVector2D<T, qualifier>::toVector4D() const noexcept { return GLMVector4D<T, qualifier>(m_data.x, m_data.y, T(0), T(1)); }
    template <number_c T, glm::qualifier qualifier>
    GLMVector4D<T, qualifier> GLMVector3D<T, qualifier>::toVector4D() const noexcept { return GLMVector4D<T, qualifier>(m_data.x, m_data.y, m_data.z, T(1)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto dot(const GLMVector2D<T, qualifier> &lhs, const GLMVector2D<T, qualifier> &rhs) noexcept
    { return glm::dot(static_cast<const glm::vec<2, T, qualifier>&>(lhs), static_cast<const glm::vec<2, T, qualifier>&>(rhs)); }
    template <number_c T, glm::qualifier qualifier>
    inline auto dot(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs) noexcept
    { return glm::dot(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs)); }
    template <number_c T, glm::qualifier qualifier>
    inline auto dot(const GLMVector4D<T, qualifier> &lhs, const GLMVector4D<T, qualifier> &rhs) noexcept
    { return glm::dot(static_cast<const glm::vec<4, T, qualifier>&>(lhs), static_cast<const glm::vec<4, T, qualifier>&>(rhs)); }
    template <number_c T, glm::qualifier qualifier>
    inline auto cross(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs) noexcept
    { return GLMVector3D<T, qualifier>(glm::cross(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs))); }
    template <number_c T, glm::qualifier qualifier>
    inline auto min(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs) noexcept
    { return GLMVector3D<T, qualifier>(glm::min(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs))); }
    template <number_c T, glm::qualifier qualifier>
    inline auto max(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs) noexcept
    { return GLMVector3D<T, qualifier>(glm::max(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs))); }
    template <number_c T, glm::qualifier qualifier>
    inline auto max(const GLMVector3D<T, qualifier> &lhs, T rhs) noexcept
    { return GLMVector3D<T, qualifier>(glm::max(static_cast<const glm::vec<3, T, qualifier>&>(lhs), rhs)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto min(const GLMVector3D<T, qualifier> &lhs, T rhs) noexcept
    { return GLMVector3D<T, qualifier>(glm::min(static_cast<const glm::vec<3, T, qualifier>&>(lhs), rhs)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto abs(const GLMVector3D<T, qualifier> &v) noexcept
    { return GLMVector3D<T, qualifier>(glm::abs(static_cast<const glm::vec<3, T, qualifier>&>(v))); }

    template <number_c T, glm::qualifier qualifier>
    inline auto normalize(const GLMVector3D<T, qualifier> &v) noexcept
    { return GLMVector3D<T, qualifier>(glm::normalize(static_cast<const glm::vec<3, T, qualifier>&>(v))); }

    template <number_c T, glm::qualifier qualifier>
    inline auto length(const GLMVector3D<T, qualifier> &v) noexcept
    { return glm::length(static_cast<const glm::vec<3, T, qualifier>&>(v)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto distance(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs) noexcept
    { return glm::distance(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto reflect(const GLMVector3D<T, qualifier> &incident, const GLMVector3D<T, qualifier> &normal) noexcept
    { return GLMVector3D<T, qualifier>(glm::reflect(static_cast<const glm::vec<3, T, qualifier>&>(incident), static_cast<const glm::vec<3, T, qualifier>&>(normal))); }

    template <number_c T, glm::qualifier qualifier>
    inline auto clamp(const GLMVector3D<T, qualifier> &v, T min_val, T max_val) noexcept
    { return GLMVector3D<T, qualifier>(glm::clamp(static_cast<const glm::vec<3, T, qualifier>&>(v), min_val, max_val)); }

    template <number_c T, glm::qualifier qualifier>
    inline auto mix(const GLMVector3D<T, qualifier> &lhs, const GLMVector3D<T, qualifier> &rhs, T t) noexcept
    { return GLMVector3D<T, qualifier>(glm::mix(static_cast<const glm::vec<3, T, qualifier>&>(lhs), static_cast<const glm::vec<3, T, qualifier>&>(rhs), t)); }

    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector2D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(const GLMVector2D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector2D<T, qualifier> && vec) noexcept { return std::move(vec[I]); }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector3D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(const GLMVector3D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector3D<T, qualifier> && vec) noexcept { return std::move(vec[I]); }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector4D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(const GLMVector4D<T, qualifier> & vec) noexcept { return vec[I]; }
    template<size_t I, number_c T, glm::qualifier qualifier>
    constexpr decltype(auto) get(GLMVector4D<T, qualifier> && vec) noexcept { return std::move(vec[I]); }
}

#include <tuple>

namespace std {
    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector2D<T, qualifier>> : integral_constant<size_t, 2> {};
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector2D<T, qualifier>> { using type = T; };
    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector3D<T, qualifier>> : integral_constant<size_t, 3> {};
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector3D<T, qualifier>> { using type = T; };
    template <lcf::number_c T, glm::qualifier qualifier>
    struct tuple_size<lcf::GLMVector4D<T, qualifier>> : integral_constant<size_t, 4> {};
    template<size_t I, lcf::number_c T, glm::qualifier qualifier>
    struct tuple_element<I, lcf::GLMVector4D<T, qualifier>> { using type = T; };
}