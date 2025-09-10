#include "Transform.h"


lcf::Transform::Transform(const Transform &other) :
    m_is_inverted_dirty(true), 
    m_scale(other.m_scale),
    m_matrix(other.m_matrix)
{
}

lcf::Transform &lcf::Transform::operator=(const Transform &other)
{
    this->requireUpdate();
    m_matrix = other.m_matrix;
    m_scale = other.m_scale;
    return *this;
}

void lcf::Transform::setLocalMatrix(const Matrix4x4 &matrix)
{
    m_matrix = matrix;
    this->requireUpdate();
}

const lcf::Matrix4x4 &lcf::Transform::getLocalMatrix() const
{
    return m_matrix;
}

const lcf::Matrix4x4 &lcf::Transform::getInvertedLocalMatrix() const
{
    if (m_is_inverted_dirty) {
        m_inverted_matrix = m_matrix.inverted();
        m_is_inverted_dirty = false;
    }
    return m_inverted_matrix;
}

void lcf::Transform::translateWorld(float x, float y, float z)
{
    this->translateWorld(Vector3D(x, y, z));
}

void lcf::Transform::translateWorld(const Vector3D &translation)
{
    if (translation.isNull()) { return; }
    m_matrix.setColumn(3, Vector4D(this->getTranslation() + translation));
    this->requireUpdate();
}

void lcf::Transform::translateLocal(float x, float y, float z)
{
    this->translateLocal(Vector3D(x, y, z));
}

void lcf::Transform::translateLocal(const Vector3D &translation)
{
    if (translation.isNull()) { return; }
    m_matrix.translateLocal(translation);
    this->requireUpdate();
}

void lcf::Transform::translateLocalXAxis(float x)
{
    this->translateLocal({x, 0.0f, 0.0f});
}

void lcf::Transform::translateLocalYAxis(float y)
{
    this->translateLocal({0.0f, y, 0.0f});
}

void lcf::Transform::translateLocalZAxis(float z)
{
    this->translateLocal({0.0f, 0.0f, z});
}

void lcf::Transform::rotateLocal(const Quaternion &rotation)
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_matrix.rotateLocal(rotation.normalized());
    this->requireUpdate();
}

void lcf::Transform::rotateLocalXAxis(float angle_degrees)
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getXAxis(), angle_degrees));
}

void lcf::Transform::rotateLocalYAxis(float angle_degrees)
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getYAxis(), angle_degrees));
}

void lcf::Transform::rotateLocalZAxis(float angle_degrees)
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getZAxis(), angle_degrees));
}

void lcf::Transform::rotateAround(const Quaternion &rotation, const Vector3D &position)
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_matrix.rotateAround(rotation, position);
    this->requireUpdate();
}

void lcf::Transform::rotateAroundOrigin(const Quaternion &rotation)
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_matrix.rotateAroundOrigin(rotation);
    this->requireUpdate();
}

void lcf::Transform::rotateAroundSelf(const Quaternion &rotation)
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_matrix.rotateAroundSelf(rotation);
    this->requireUpdate();
}

void lcf::Transform::scale(float factor)
{
    this->scale(Vector3D(factor, factor, factor));
}

void lcf::Transform::scale(float x, float y, float z)
{
    this->scale(Vector3D(x, y, z));
}

void lcf::Transform::scale(const Vector3D &scale)
{
    Vector3D s = max(scale, std::numeric_limits<float>::epsilon());
    m_matrix.scale(s);
    m_scale *= s;
    this->requireUpdate();
}

void lcf::Transform::setTranslation(float x, float y, float z)
{
    this->setTranslation(Vector3D(x, y, z));
}

void lcf::Transform::setTranslation(const Vector3D &position)
{
    m_matrix.setColumn(3, Vector4D(position, 1.0f));
    this->requireUpdate();
}

void lcf::Transform::setRotation(const Quaternion &rotation)
{
    m_matrix.setRotation(rotation);
    this->requireUpdate();
}

void lcf::Transform::setRotation(float angle_degrees, float x, float y, float z)
{
    this->setRotation(angle_degrees, Vector3D(x, y, z));
}

void lcf::Transform::setRotation(float angle_degrees, const Vector3D &axis)
{
    this->setRotation(Quaternion::fromAxisAndAngle(axis.normalized(), angle_degrees));
}

void lcf::Transform::setScale(float x, float y, float z)
{
    this->setScale(Vector3D(x, y, z));
}

void lcf::Transform::setScale(const Vector3D &scale)
{
    m_scale = max(scale, std::numeric_limits<float>::epsilon());
    m_matrix.setScale(m_scale);
    this->requireUpdate();
}

void lcf::Transform::setScale(float factor)
{
    this->setScale(Vector3D(factor, factor, factor));
}

void lcf::Transform::setTRS(const Vector3D &translation, const Quaternion &rotation, const Vector3D &scale)
{
    m_scale = max(scale, std::numeric_limits<float>::epsilon());
    this->setTranslation(translation);
    this->setRotation(rotation);
    this->setScale(scale);
}

lcf::Vector3D lcf::Transform::getXAxis() const
{
    return this->getLocalMatrix().column(0).toVector3D();
}

lcf::Vector3D lcf::Transform::getYAxis() const
{
    return this->getLocalMatrix().column(1).toVector3D();
}

lcf::Vector3D lcf::Transform::getZAxis() const
{
    return this->getLocalMatrix().column(2).toVector3D();
}

lcf::Vector3D lcf::Transform::getTranslation() const
{
    return m_matrix.column(3).toVector3D();
}

lcf::Quaternion lcf::Transform::getRotation() const
{
    return m_matrix.getRotation();
}

lcf::Vector3D lcf::Transform::getScale() const
{
    return m_scale;
}

void lcf::Transform::requireUpdate()
{
    m_is_inverted_dirty = true;
}