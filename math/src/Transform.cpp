#include "Transform.h"

using namespace lcf;

lcf::Transform::Transform(const Transform &other) :
    m_local_matrix(other.m_local_matrix),
    m_world_matrix(other.m_world_matrix)
{
}

lcf::Transform::Transform(Transform &&other) noexcept :
    m_local_matrix(std::move(other.m_local_matrix)),
    m_world_matrix(std::move(other.m_world_matrix))
{
}

lcf::Transform & lcf::Transform::operator=(const Transform &other)
{

    m_local_matrix = other.m_local_matrix;
    m_world_matrix = other.m_world_matrix;
    return *this;
}

lcf::Transform & lcf::Transform::operator=(Transform &&other) noexcept
{
    m_local_matrix = std::move(other.m_local_matrix);
    m_world_matrix = std::move(other.m_world_matrix);
    return *this;
}

void lcf::Transform::setLocalMatrix(const Matrix4x4 &matrix) noexcept
{
    m_local_matrix = matrix;
}

void lcf::Transform::setWorldMatrix(const Matrix4x4 & world_matrix) noexcept
{
    m_world_matrix = world_matrix;
}

void lcf::Transform::setWorldMatrix(Matrix4x4 &&world_matrix) noexcept
{
    m_world_matrix = std::move(world_matrix);
}

void lcf::Transform::translateWorld(float x, float y, float z) noexcept
{
    this->translateWorld(Vector3D<float>(x, y, z));
}

void lcf::Transform::translateWorld(const Vector3D<float> &translation) noexcept
{
    if (translation.isNull()) { return; }
    m_local_matrix.setColumn(3, Vector4D<float>(this->getTranslation() + translation, 1.0f));;
}

void lcf::Transform::translateLocal(float x, float y, float z) noexcept
{
    this->translateLocal(Vector3D<float>(x, y, z));
}

void lcf::Transform::translateLocal(const Vector3D<float> &translation) noexcept
{
    if (translation.isNull()) { return; }
    m_local_matrix.translateLocal(translation);
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

void lcf::Transform::rotateAround(const Quaternion &rotation, const Vector3D<float> &position) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAround(rotation, position);
}

void lcf::Transform::rotateAroundOrigin(const Quaternion &rotation) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAroundOrigin(rotation);
}

void lcf::Transform::rotateAroundSelf(const Quaternion &rotation) noexcept
{
    if (rotation.isNull() or rotation.isIdentity()) { return; }
    m_local_matrix.rotateAroundSelf(rotation);
}

void lcf::Transform::scale(float factor) noexcept
{
    this->scale(Vector3D<float>(factor, factor, factor));
}

void lcf::Transform::scale(float x, float y, float z) noexcept
{
    this->scale(Vector3D<float>(x, y, z));
}

void lcf::Transform::scale(const Vector3D<float> &scale) noexcept
{
    m_local_matrix.scale(max(scale, std::numeric_limits<float>::epsilon()));
}

void lcf::Transform::setTranslation(float x, float y, float z) noexcept
{
    this->setTranslation(Vector3D<float>(x, y, z));
}

void lcf::Transform::setTranslation(const Vector3D<float> &position) noexcept
{
    m_local_matrix.setColumn(3, Vector4D<float>(position, 1.0f));
}

void lcf::Transform::setRotation(const Quaternion &rotation) noexcept
{
    m_local_matrix.setRotation(rotation);
}

void lcf::Transform::setRotation(float angle_degrees, float x, float y, float z) noexcept
{
    this->setRotation(angle_degrees, Vector3D<float>(x, y, z));
}

void lcf::Transform::setRotation(float angle_degrees, const Vector3D<float> &axis) noexcept
{
    this->setRotation(Quaternion::fromAxisAndAngle(axis.normalized(), angle_degrees));
}

void lcf::Transform::setScale(float x, float y, float z) noexcept
{
    this->setScale(max(Vector3D<float>(x, y, z), std::numeric_limits<float>::epsilon()));
}

void lcf::Transform::setScale(const Vector3D<float> &scale) noexcept
{
    m_local_matrix.setScale(scale);
}

void lcf::Transform::setScale(float factor) noexcept
{
    this->setScale(Vector3D<float>(factor, factor, factor));
}

void lcf::Transform::setTRS(const Vector3D<float> &translation, const Quaternion &rotation, const Vector3D<float> &scale) noexcept
{
    this->setTranslation(translation);
    this->setRotation(rotation);
    this->setScale(scale);
}

lcf::Vector3D<float> lcf::Transform::getXAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(0).toVector3D();
}

lcf::Vector3D<float> lcf::Transform::getYAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(1).toVector3D();
}

lcf::Vector3D<float> lcf::Transform::getZAxis() const noexcept
{
    return this->getLocalMatrix().getColumn(2).toVector3D();
}

lcf::Vector3D<float> lcf::Transform::getTranslation() const noexcept
{
    return m_local_matrix.getColumn(3).toVector3D();
}

lcf::Quaternion lcf::Transform::getRotation() const noexcept
{
    return m_local_matrix.getRotation();
}

lcf::Vector3D<float> lcf::Transform::getScale() const noexcept
{
    return m_local_matrix.getScale();
}
