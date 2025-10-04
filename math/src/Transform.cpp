#include "Transform.h"


lcf::Transform::Transform(const Transform &other) :
    m_is_inverted_dirty(other.m_is_inverted_dirty), 
    m_local_matrix(other.m_local_matrix),
    m_world_matrix(other.m_world_matrix),
    m_inverted_world_matrix(other.m_inverted_world_matrix)
{
}

lcf::Transform &lcf::Transform::operator=(const Transform &other)
{

    m_local_matrix = other.m_local_matrix;
    m_world_matrix = other.m_world_matrix;
    m_inverted_world_matrix = other.m_inverted_world_matrix;
    m_is_inverted_dirty = other.m_is_inverted_dirty;
    return *this;
}

void lcf::Transform::setLocalMatrix(const Matrix4x4 &matrix) noexcept
{
    m_local_matrix = matrix;
}

void lcf::Transform::setWorldMatrix(const Matrix4x4 *world_matrix) noexcept
{
    if (not world_matrix) { return; }
    m_world_matrix = world_matrix;
    this->markDirty();
}

const lcf::Matrix4x4 &lcf::Transform::getInvertedWorldMatrix() const noexcept
{
    if (m_is_inverted_dirty) {
        m_inverted_world_matrix = m_world_matrix->inverted();
        m_is_inverted_dirty = false;
    }
    return m_inverted_world_matrix;
}

void lcf::Transform::translateWorld(float x, float y, float z) noexcept
{
    this->translateWorld(Vector3D(x, y, z));
}

void lcf::Transform::translateWorld(const Vector3D &translation) noexcept
{
    if (translation.isNull()) { return; }
    m_local_matrix.setColumn(3, Vector4D(this->getTranslation() + translation, 1.0f));
    this->markDirty();
}

void lcf::Transform::translateLocal(float x, float y, float z) noexcept
{
    this->translateLocal(Vector3D(x, y, z));
}

void lcf::Transform::translateLocal(const Vector3D &translation) noexcept
{
    if (translation.isNull()) { return; }
    m_local_matrix.translateLocal(translation);
    this->markDirty();
}

void lcf::Transform::translateLocalXAxis(float x) noexcept
{
    this->translateLocal({x, 0.0f, 0.0f});
}

void lcf::Transform::translateLocalYAxis(float y) noexcept
{
    this->translateLocal({0.0f, y, 0.0f});
}

void lcf::Transform::translateLocalZAxis(float z) noexcept
{
    this->translateLocal({0.0f, 0.0f, z});
}

void lcf::Transform::rotateLocal(const Quaternion &rotation) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateLocal(rotation.normalized());
    this->markDirty();
}

void lcf::Transform::rotateLocalXAxis(float angle_degrees) noexcept
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getXAxis(), angle_degrees));
}

void lcf::Transform::rotateLocalYAxis(float angle_degrees) noexcept
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getYAxis(), angle_degrees));
}

void lcf::Transform::rotateLocalZAxis(float angle_degrees) noexcept
{
    this->rotateLocal(Quaternion::fromAxisAndAngle(this->getZAxis(), angle_degrees));
}

void lcf::Transform::rotateAround(const Quaternion &rotation, const Vector3D &position) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAround(rotation, position);
    this->markDirty();
}

void lcf::Transform::rotateAroundOrigin(const Quaternion &rotation) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAroundOrigin(rotation);
    this->markDirty();
}

void lcf::Transform::rotateAroundSelf(const Quaternion &rotation) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAroundSelf(rotation);
    this->markDirty();
}

void lcf::Transform::scale(float factor) noexcept
{
    this->scale(Vector3D(factor, factor, factor));
}

void lcf::Transform::scale(float x, float y, float z) noexcept
{
    this->scale(Vector3D(x, y, z));
}

void lcf::Transform::scale(const Vector3D &scale) noexcept
{
    m_local_matrix.scale(max(scale, std::numeric_limits<float>::epsilon()));
    this->markDirty();
}

void lcf::Transform::setTranslation(float x, float y, float z) noexcept
{
    this->setTranslation(Vector3D(x, y, z));
}

void lcf::Transform::setTranslation(const Vector3D &position) noexcept
{
    m_local_matrix.setColumn(3, Vector4D(position, 1.0f));
    this->markDirty();
}

void lcf::Transform::setRotation(const Quaternion &rotation) noexcept
{
    m_local_matrix.setRotation(rotation);
    this->markDirty();
}

void lcf::Transform::setRotation(float angle_degrees, float x, float y, float z) noexcept
{
    this->setRotation(angle_degrees, Vector3D(x, y, z));
}

void lcf::Transform::setRotation(float angle_degrees, const Vector3D &axis) noexcept
{
    this->setRotation(Quaternion::fromAxisAndAngle(axis.normalized(), angle_degrees));
}

void lcf::Transform::setScale(float x, float y, float z) noexcept
{
    this->setScale(max(Vector3D(x, y, z), std::numeric_limits<float>::epsilon()));
}

void lcf::Transform::setScale(const Vector3D &scale) noexcept
{
    m_local_matrix.setScale(scale);
    this->markDirty();
}

void lcf::Transform::setScale(float factor) noexcept
{
    this->setScale(Vector3D(factor, factor, factor));
}

void lcf::Transform::setTRS(const Vector3D &translation, const Quaternion &rotation, const Vector3D &scale) noexcept
{
    this->setTranslation(translation);
    this->setRotation(rotation);
    this->setScale(scale);
}

lcf::Vector3D lcf::Transform::getXAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(0).toVector3D();
}

lcf::Vector3D lcf::Transform::getYAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(1).toVector3D();
}

lcf::Vector3D lcf::Transform::getZAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(2).toVector3D();
}

lcf::Vector3D lcf::Transform::getTranslation() const noexcept
{
    return m_local_matrix.getColumn(3).toVector3D();
}

lcf::Quaternion lcf::Transform::getRotation() const noexcept
{
    return m_local_matrix.getRotation();
}

lcf::Vector3D lcf::Transform::getScale() const noexcept
{
    return m_local_matrix.getScale();
}