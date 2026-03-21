// #pragma once

// #include "ResourceEntity.h"
// #include "resources_enums.h"
// #include "resources_signals.h"

// namespace lcf {
//     template <typename Resource>
//     class ResourceHandle
//     {
//     public:
//         ResourceHandle() = default;
//         ResourceHandle(ResourceRegistry & registry) : m_entity(registry) {}
//         ~ResourceHandle() noexcept = default;
//         ResourceHandle(const ResourceHandle & other) noexcept = default;
//         ResourceHandle & operator=(const ResourceHandle & other) noexcept = default;
//         ResourceHandle(ResourceHandle && other) noexcept = default;
//         ResourceHandle & operator=(ResourceHandle && other) noexcept = default;
//         operator bool() const noexcept { return this->isValid(); }
//         Resource & operator*() const noexcept { return m_entity.get<Resource>(); }
//         Resource * operator->() const noexcept { return &m_entity.get<Resource>(); }
//         bool isValid() const noexcept { return m_entity.isValid(); }
//     public:
//         ResourceLease getLease() const noexcept { return m_entity.getLease(); }
//         ResourceArtifactID getArtifactID() const noexcept{ return m_entity.getArtifactID(); }
//         ResourceState getState() const noexcept { return m_entity.get<ResourceState>(); }
//     private:
//         ResourceEntity m_entity;
//     };
// }