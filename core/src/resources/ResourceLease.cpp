#include "core/resources/ResourceLease.h"
#include "core/resources/ResourceEntity.h"

using namespace lcf;

ResourceLease::ResourceLease(ResourceEntityLifecycle & entity_lifecycle) : m_entity_lifecycle_p(&entity_lifecycle)
{
    if (m_entity_lifecycle_p) { m_entity_lifecycle_p->increaseTenantCount(); }
}

ResourceLease::~ResourceLease() noexcept
{
    this->tryDestroy();
}

ResourceLease::ResourceLease(const ResourceLease & other) noexcept :
    m_entity_lifecycle_p(other.m_entity_lifecycle_p)
{
    if (m_entity_lifecycle_p) { m_entity_lifecycle_p->increaseTenantCount(); }
}

ResourceLease & ResourceLease::operator=(const ResourceLease & other) noexcept
{
    if (this == &other) { return *this; }
    this->tryDestroy();
    m_entity_lifecycle_p = other.m_entity_lifecycle_p;
    if (m_entity_lifecycle_p) { m_entity_lifecycle_p->increaseTenantCount(); }
    return *this;
}

ResourceLease::ResourceLease(ResourceLease && other) noexcept :
    m_entity_lifecycle_p(std::exchange(other.m_entity_lifecycle_p, nullptr))
{
}

ResourceLease & ResourceLease::operator=(ResourceLease && other) noexcept
{
    if (this == &other) { return *this; }
    this->tryDestroy();
    m_entity_lifecycle_p = std::exchange(other.m_entity_lifecycle_p, nullptr);
    return *this;
}

void ResourceLease::tryDestroy() noexcept
{
    if (not m_entity_lifecycle_p) { return; }
    m_entity_lifecycle_p->decreaseOwnerCount();
    if (m_entity_lifecycle_p->shouldDestroy()) {
        delete m_entity_lifecycle_p;
        m_entity_lifecycle_p = nullptr;
    }
}
