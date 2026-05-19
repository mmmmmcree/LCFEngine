#include "RendererSwitcher.h"

#include "BenchmarkScene.h"
#include "CpuIndirectRenderer.h"
#include "GpuDrivenRenderer.h"
#include "NaiveCpuRenderer.h"
#include "Vulkan/VulkanContext.h"
#include "log.h"

namespace lcf::benchmark {

    RendererSwitcher::~RendererSwitcher()
    {
        if (m_context_p) {
            m_context_p->getDevice().waitIdle();
        }
        // m_renderers 的 unique_ptr 析构会自动按各自路径销毁。
    }

    void RendererSwitcher::create(
        render::VulkanContext * context,
        BenchmarkScene * scene,
        std::pair<uint32_t, uint32_t> max_extent,
        ecs::Registry & registry)
    {
        m_context_p   = context;
        m_scene_p     = scene;
        m_registry_p  = &registry;
        m_max_extent  = max_extent;
    }

    void RendererSwitcher::switchPath(ePath path)
    {
        if (m_active == path and m_renderers[std::to_underlying(path)]) { return; }
        if (m_context_p) { m_context_p->getDevice().waitIdle(); }

        // 切换 path 前清掉 BenchmarkScene 中的 host shadow 污染（CPU 路径会改写
        // m_draw_meta_infos / m_visible_instances，GPU 路径若读到这些状态会破坏对照公平）。
        if (m_scene_p) {
            m_scene_p->resetDrawMetaToOriginal();
            m_scene_p->clearVisibleInstancesShadow();
        }

        const auto idx = std::to_underlying(path);
        if (not m_renderers[idx]) {
            auto rp = this->makeRenderer(path);
            if (not rp) {
                lcf_log_warn("RendererSwitcher::switchPath: renderer for path={} not implemented yet",
                             to_csv_name(path));
                return;
            }
            rp->create(m_context_p, m_scene_p, m_max_extent, *m_registry_p);
            m_renderers[idx] = std::move(rp);
        }
        m_active = path;
        lcf_log_info("RendererSwitcher: active path = {}", to_csv_name(m_active));
    }

    FrameMetrics RendererSwitcher::render(
        const ecs::Entity & camera, const ecs::Entity & render_target)
    {
        const auto idx = std::to_underlying(m_active);
        if (not m_renderers[idx]) { return {}; }
        return m_renderers[idx]->render(camera, render_target);
    }

    IBenchmarkRenderer * RendererSwitcher::getRenderer(ePath path) noexcept
    {
        return m_renderers[std::to_underlying(path)].get();
    }

    std::unique_ptr<IBenchmarkRenderer> RendererSwitcher::makeRenderer(ePath path)
    {
        switch (path) {
            case ePath::eGpuDriven:         return std::make_unique<GpuDrivenRenderer>();
            case ePath::eCpuDrivenIndirect: return std::make_unique<CpuIndirectRenderer>();
            case ePath::eCpuDrivenNaive:    return std::make_unique<NaiveCpuRenderer>();
        }
        return nullptr;
    }

}  // namespace lcf::benchmark

