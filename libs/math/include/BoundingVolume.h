#pragma once

#include "Vector.h"
#include "Frustum.h"
#include "concepts/range_concept.h"
#include <ranges>

namespace lcf {
    template <number_c T>
    class BoundingSphere
    {
        using Self = BoundingSphere<T>;
        using Vec3 = Vector3D<T>;
    public:
        BoundingSphere() noexcept = default;
        ~BoundingSphere() noexcept = default;
        BoundingSphere(const Vec3 & center, T radius) noexcept :
            m_center(center),
            m_radius(radius)
        {}
        BoundingSphere(convertible_range_of_c<Vec3> auto && positions)
        {
            Vec3 min_pos, max_pos;
            min_pos.fill(std::numeric_limits<T>::max());
            max_pos.fill(std::numeric_limits<T>::lowest());
            for (const auto & pos : positions) {
                m_center += pos;
                min_pos = min(min_pos, pos);
                max_pos = max(max_pos, pos);
            }
            m_center /= static_cast<T>(std::ranges::size(positions));
            m_radius = (max_pos - m_center).length() / static_cast<T>(2.0f);
        }
        BoundingSphere(const Self & other) noexcept = default;
        BoundingSphere(Self && other) noexcept = default;
        Self & operator=(const Self & other) noexcept = default;
        Self & operator=(Self && other) noexcept = default;
    public:
        bool isInside(const Frustum<T> & frustum) const noexcept
        {
            return frustum.isSphereInside(m_center, m_radius);
        }
        bool isIntersecting(const Frustum<T> & frustum) const noexcept
        {
            return frustum.isSphereIntersecting(m_center, m_radius);
        }
        const Vec3 & getCenter() const { return m_center; }
        T getRadius() const { return m_radius; }
    private:
        Vec3 m_center {};
        T m_radius {};
    };
}