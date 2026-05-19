#pragma once

// RendererSwitcher：在三个 IBenchmarkRenderer 之间安全切换。
//
// 用法：
//   RendererSwitcher s;
//   s.create(context, scene, max_extent, registry);
//   s.switchPath(ePath::eGpuDriven);
//   s.render(camera, render_target);   // 转发给当前 active renderer
//   s.switchPath(ePath::eCpuDrivenIndirect);
//   ...
//
// 切换过程：
//   1) device.waitIdle() — 保证旧 renderer 没有 in-flight 命令缓冲；
//   2) 切换 active 指针；旧 renderer 的 frame_resources 仍保留，便于循环切换不重建。

#include "IBenchmarkRenderer.h"

#include <array>
#include <memory>
#include <utility>

namespace lcf::benchmark {

    class RendererSwitcher
    {
    public:
        RendererSwitcher() = default;
        ~RendererSwitcher();
        RendererSwitcher(const RendererSwitcher &) = delete;
        RendererSwitcher & operator=(const RendererSwitcher &) = delete;

        void create(render::VulkanContext * context,
                    BenchmarkScene * scene,
                    std::pair<uint32_t, uint32_t> max_extent,
                    ecs::Registry & registry);

        // 切换到指定路径；若该路径的 renderer 未创建，立即创建。
        // 切换前会 device.waitIdle 保证旧 renderer 没有 in-flight 命令。
        void switchPath(ePath path);

        ePath getActivePath() const noexcept { return m_active; }

        // 转发渲染调用。若当前没有 active renderer 返回空 FrameMetrics。
        FrameMetrics render(const ecs::Entity & camera,
                            const ecs::Entity & render_target);

        IBenchmarkRenderer * getRenderer(ePath path) noexcept;

    private:
        std::unique_ptr<IBenchmarkRenderer> makeRenderer(ePath path);

    private:
        render::VulkanContext * m_context_p   = nullptr;
        BenchmarkScene        * m_scene_p     = nullptr;
        ecs::Registry         * m_registry_p  = nullptr;
        std::pair<uint32_t, uint32_t> m_max_extent {};

        std::array<std::unique_ptr<IBenchmarkRenderer>, k_path_count> m_renderers;
        ePath m_active = ePath::eGpuDriven;
    };

}  // namespace lcf::benchmark
