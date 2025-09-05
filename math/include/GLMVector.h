#pragma once

#include "glm.h"
#include <string>

namespace lcf {
    class GLMVector3D;
    class GLMVector4D;
    class GLMVector2D;

    class GLMVector2D : public glm::vec2
    {
    public:
        GLMVector2D() : glm::vec2(0.0f, 0.0f) {}
        GLMVector2D(float x, float y) : glm::vec2(x, y) {}
    };

    class GLMVector3D : public glm::vec3
    {
    public:
        static GLMVector3D crossProduct(const GLMVector3D &lhs, const GLMVector3D &rhs);
        static float dotProduct(const GLMVector3D &lhs, const GLMVector3D &rhs);
        GLMVector3D() : glm::vec3(0.0f, 0.0f, 0.0f) {}
        GLMVector3D(float x, float y, float z);
        GLMVector3D(const glm::vec3 &vec);
        GLMVector3D(const GLMVector4D &vec);
        GLMVector3D &operator=(const glm::vec3 &vec);
        GLMVector3D operator+(const GLMVector3D &vec) const;
        GLMVector3D operator-(const GLMVector3D &vec) const;
        GLMVector3D operator*(const GLMVector3D &vec) const;
        GLMVector3D operator/(const GLMVector3D &vec) const;
        GLMVector3D operator*(float scalar) const;
        GLMVector3D operator/(float scalar) const;
        GLMVector3D &operator+=(const GLMVector3D &vec);
        GLMVector3D &operator-=(const GLMVector3D &vec);
        GLMVector3D &operator*=(const GLMVector3D &vec);
        GLMVector3D &operator/=(const GLMVector3D &vec);
        GLMVector3D &operator*=(float scalar);
        GLMVector3D &operator/=(float scalar);
        void setX(float x);
        float getX() const;
        void setY(float y);
        float getY() const;
        void setZ(float z);
        float getZ() const;
        GLMVector4D toVector4D() const;
        void normalize();
        GLMVector3D normalized() const;
        bool isNull() const;
    };

    class GLMVector4D : public glm::vec4
    {
    public:
        GLMVector4D() : glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) {};
        GLMVector4D(float x, float y, float z, float w);
        GLMVector4D(const GLMVector3D &vec, float w);
        GLMVector4D(const glm::vec4 &vec);
        GLMVector4D &operator=(const glm::vec4 &vec);
        GLMVector4D(const GLMVector3D &vec);
        void setX(float x);
        float getX() const;
        void setY(float y);
        float getY() const;
        void setZ(float z);
        float getZ() const;
        void setW(float w);
        float getW() const; 
        GLMVector3D toVector3D() const;
        void normalize();
        GLMVector4D normalized() const;
        bool isNull() const;
    };

    std::string to_string(const GLMVector3D &vec);

    std::string to_string(const GLMVector4D &vec);
}