#pragma once

#include "Vector.h"
#include "Matrix.h"
#include "concepts/number_concept.h"
#include <utility>
#include <array>

namespace lcf {

    template<number_c T>
    class Frustum
    {
        using Self = Frustum<T>;
        using Vec3 = Vector3D<T>;
        using Vec4 = Vector4D<T>;
        using Mat4 = Matrix4x4<T>;
    public:
        enum class Side
        {
            eLeft = 0,
            eRight = 1,
            eTop = 2,
            eBottom = 3,
            eBack = 4,
            eFront = 5
        };
    public:
        Frustum() noexcept = default;
        ~Frustum() noexcept = default;
        Frustum(const Self & other) = default;
        Frustum(Self && other) noexcept = default;
        Self & operator=(const Self & other) = default;
        Self & operator=(Self && other) noexcept = default;
    public:
        void update(const Mat4 & mat) noexcept
        {
            auto row0 = mat.getRow(0);
            auto row1 = mat.getRow(1);
            auto row2 = mat.getRow(2);
            auto row3 = mat.getRow(3);
            this->getPlane(Side::eLeft) = row3 + row0;
            this->getPlane(Side::eRight) = row3 - row0;
            this->getPlane(Side::eTop) = row3 - row1;
            this->getPlane(Side::eBottom) = row3 + row1;
            this->getPlane(Side::eBack) = row3 + row2;
            this->getPlane(Side::eFront) = row3 - row2;
            for (auto & plane : m_planes) { plane.normalize(); }
        }
        bool isSphereInside(const Vec3 & center, T radius) const noexcept
        {
            return std::ranges::all_of(m_planes, [center, radius](const auto & plane) {
                return dot(plane, Vec4(center, 1.0f)) + radius > 0;
            });
        }
        bool isSphereIntersecting(const Vec3 & center, T radius) const noexcept
        {
            return std::ranges::any_of(m_planes, [center, radius](const auto & plane) {
                return dot(plane, Vec4(center, 1.0f)) + radius > 0;
            });
        }
        Vec4 & getPlane(Side side) noexcept { return m_planes[std::to_underlying(side)]; }
        const Vec4 & getPlane(Side side) const noexcept { return m_planes[std::to_underlying(side)]; }
    private:
        std::array<Vec4, 6> m_planes;
    };
}