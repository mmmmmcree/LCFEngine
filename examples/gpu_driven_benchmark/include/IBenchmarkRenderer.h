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

        // 设置模拟模式（默认空实现）：
        //   - NaiveCpu / CpuIndirect / GpuDriven 各自 override 实现 mode 内部分支
        //   - 不识别的 mode 静默忽略
        // 切换时机：必须在两次 render() 之间调用；renderer 内部下一帧 record 即生效。
        virtual void setEmulationMode(eEmulationMode /*mode*/) {}

        // 设置剔除开关（默认空实现）：
        //   - false：正常 6 平面 frustum test
        //   - true：恒等填充（visible = 全部 instance），用于 chap05 §5.5.2 ABL-CULL 消融
        // GpuDriven 通过 push_const 让 cull.comp 跳过 frustum test；CPU 路径直接走 identity 分支。
        virtual void setDisableCull(bool /*disabled*/) {}
    };

}  // namespace lcf::benchmark
