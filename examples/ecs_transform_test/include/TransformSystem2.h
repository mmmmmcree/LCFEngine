#pragma once

#include "Entity.h"
#include "Registry.h"
#include "OOPTransform.h"

namespace lcf {
    struct OOPTransform2UpdateSignalInfo : public EntitySignalBase
    {

    };
    struct OOPTransform2AttachSignalInfo : public EntitySignalBase
    {
        OOPTransform2AttachSignalInfo(EntityHandle parent) : m_parent(parent) {}
        EntityHandle m_parent;
    };

    struct TransformSystem2
    {
        TransformSystem2(Registry & registry) : m_registry_p(&registry)
        {
            auto & dispatcher = m_registry_p->ctx().get<Dispatcher>();
            dispatcher.sink<OOPTransform2UpdateSignalInfo>().connect<&TransformSystem2::onTransform2Update>(*this);
            dispatcher.sink<OOPTransform2AttachSignalInfo>().connect<&TransformSystem2::onTransform2Attach>(*this);
        }
        void onTransform2Attach(const OOPTransform2AttachSignalInfo & info)
        {
            auto entity = info.m_sender;
            auto parent = info.m_parent;
            auto & h = m_registry_p->get<OOPTransform2Hierarchy>(parent);
            h.m_children.push_back(entity);
            auto & t = m_registry_p->get<OOPTransform2>(entity);
            auto & p_t = m_registry_p->get<OOPTransform2>(parent);
            t.m_parent = &p_t;
        }
        void onTransform2Update(const OOPTransform2UpdateSignalInfo & info)
        {
            auto entity = info.m_sender;
            this->markDirty(entity);
        }
        void markDirty(EntityHandle entity)
        {
            auto & t = m_registry_p->get<OOPTransform2>(entity);
            if (t.m_is_dirty) { return; }
            t.m_is_dirty = true;
            auto & h = m_registry_p->get<OOPTransform2Hierarchy>(entity);
            for (auto child : h.m_children) {
                this->markDirty(child);
            }
        }
        void update() noexcept
        {
            for (auto [entity, transform] : m_registry_p->view<lcf::OOPTransform2>().each()) {
                transform.getWorldMatrix();
            }
        }
        Registry * m_registry_p = nullptr;
    };

}