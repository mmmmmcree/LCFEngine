#pragma once

// IBenchmarkRenderer：三种渲染路径的共同抽象接口。
//
// 设计原则：
//   1) 每个 renderer 自管理自己的命令缓冲、管线、frame_resources 与 timestamp 查询池；
//   2) 共享 BenchmarkScene 中的模型/材质/纹理/UBO/SSBO/bindless DS（指针传入，不拥有）；
//   3) 每帧 render 把 M1（CPU 提交耗时）/ M2（GPU 帧时间）/ M3（绘制调用数）
//      / M4（GPU 剔除耗时，仅 GpuDriven 非零）写到 out_metrics；
//   4) 析构时由 renderer 自身保证已 device.waitIdle，不让 GPU 仍持有命令缓冲。

#include "benchmark_types.h"

#include <cstdint>
#include <utility>

namespace lcf::ecs {
    class Entity;
    class Registry;
}
namespace lcf::render { class VulkanContext; }

namespace lcf::benchmark {

    class BenchmarkScene;

    class IBenchmarkRenderer
    {
    public:
        virtual ~IBenchmarkRenderer() = default;

        // 一次性 create：分配管线、frame_resources、timestamp pool 等。
        // max_extent 决定 framebuffer / 中间附件的最大尺寸（与 main_example 一致）。
        virtual void create(render::VulkanContext * context,
                            BenchmarkScene * scene,
                            std::pair<uint32_t, uint32_t> max_extent,
                            ecs::Registry & registry) = 0;

        // 渲染一帧。返回该帧的 M1/M2/M3/M4 指标。
        // 注意：M2/M4 是「上一轮同帧位回读」的值——首帧返回 0；这是 GPU 时间戳的常见模式。
        virtual FrameMetrics render(const ecs::Entity & camera,
                                    const ecs::Entity & render_target) = 0;

        virtual ePath getPath() const noexcept = 0;
    };

}  // namespace lcf::benchmark
