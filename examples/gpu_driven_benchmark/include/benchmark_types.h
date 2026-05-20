#pragma once

// gpu_driven_benchmark：跨文件共享的小型 POD 与枚举集合。
//
// 这里不放任何实现细节，仅放：
//   1) 三种渲染路径与四档场景的枚举；
//   2) 单帧采集的 5 个指标（与 chap05 表 M1-M5 一一对应）；
//   3) 与 GLSL 端 bindless_structs.glsl 二进制布局一一对应的 host 端结构体
//      （CameraData / ObjectData / InstanceData / DrawMetaInfo），
//      字段顺序必须与 RenderEngine/src/Vulkan/VulkanRenderer.cpp 中私有定义保持一致；
//   4) 论文 chap05 三路径名 → 内部枚举名映射，用于 CSV path 列。

#include <array>
#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.hpp>

#include "Frustum.h"
#include "Matrix.h"
#include "Vector.h"

namespace lcf::benchmark {

    // 渲染路径枚举（与 chap05 §5.4.1 的三路径一一对应）。
    enum class ePath : uint8_t
    {
        eCpuDrivenNaive    = 0,  // 朴素 CPU 绘制路径
        eCpuDrivenIndirect = 1,  // CPU 生成间接命令路径
        eGpuDriven         = 2,  // GPU 生成绘制路径（本文核心方案）
    };

    constexpr std::size_t k_path_count = 3;

    // 模拟模式枚举：与 ePath 正交的"路径内 host 行为变体"。
    // 用于在不增加 ePath 数量的前提下，对单条 path 提供"工业悲观 baseline" vs
    // "理想化 bindless 写法"两档对照（chap05 §5.4.2 表格里的优化梯度）。
    //
    // 每条 path 的可用 mode 集合由 RendererSwitcher 决定：
    //   eCpuDrivenNaive    → { eClean, eLegacy }
    //   eCpuDrivenIndirect → { eSingle, eLegacy }
    //   eGpuDriven         → { eGpuDriven, eGpuIndirectCount }（后者用于 ABL-DGC 消融）
    //
    // 注：eLegacy 在 NaiveCpu 与 CpuIndirect 上语义不同但精神一致——都对应"工业悲观/
    // 现实"形态：放弃 BDA + Bindless 基础设施带来的"一次绑定全部绘制"红利，主动引入
    // PSO 切换 + 全 ds rebind 的 host overhead。两者切换粒度不同（Naive 在 instance
    // 粒度、Indirect 在 mesh 粒度），反映批量提交方式的本质差异，详见论文 §5.4.1。
    enum class eEmulationMode : uint8_t
    {
        eClean            = 0,  // NaiveCpu: 现状（per-mesh push_constants + per-instance vkCmdDraw）
        eLegacy           = 1,  // 双路径共用：周期性 PSO toggle + 切换后强制全 ds rebind + push_const 重发；
                                //   NaiveCpu  → 每 m_pipeline_switch_period (默认 4096) 个 instance 切一次
                                //   CpuIndirect → 每 mesh 必切（drawIndirect 之间 toggle PSO）
        eSingle           = 2,  // CpuIndirect: 现状（1 次 vkCmdDrawIndirectCount 全包，享受 BDA+Bindless 红利）
        eGpuDriven        = 4,  // GpuDriven: DGC + GPU cull（核心方案）
        eGpuIndirectCount = 5,  // GpuDriven: GPU cull 仍开，但用 vkCmdDrawIndirectCount 替 DGC（ABL-DGC 消融）
    };

    // 场景规模档位（与 chap05 主对照矩阵的 A/B/C/D 列对齐）。
    enum class eScene : uint8_t
    {
        eA = 0,  // 低实例规模
        eB = 1,  // 中实例规模
        eC = 2,  // 高实例规模
        eD = 3,  // 极大实例规模
    };

    constexpr std::size_t k_scene_count = 4;

    // 默认每个 mesh 的实例倍率（每档对应一个 mesh 的 instance 数）。
    // CLI 可通过 `--instance A=64,B=256,...` 在运行时覆盖该数组。
    constexpr std::array<uint32_t, k_scene_count> k_default_instance_per_mesh = {
        64u,    // A
        256u,   // B
        1024u,  // C
        4096u,  // D
    };

    // 每帧采集的指标（与 chap05 §5.4.2 表头 M1/M2/M3/M4/M5 对应）。
    // 注意：M5（p99）不是逐帧值，而是基于 m2 序列在一次跑分窗口结束时计算得到，
    // 因此不在该结构体里。逐帧仅 m1..m4。
    //
    // M1/M4 的语义切分（统一论文对照口径）：
    //   - m1_cpu_submit_ms：CPU 端 record + transfer + submit + finishRender 等"非剔除"段
    //                       不再包含 cull 时间，避免 CPU 路径的 cull 被吃进 M1、与
    //                       GpuDriven 的 cull（在 GPU）位置不对称导致比较失真。
    //   - m4_cull_ms：剔除阶段耗时（不区分 CPU/GPU 路径）：
    //                  · NaiveCpu / CpuIndirect → host chrono 包住 cullOnCpu()
    //                  · GpuDriven              → GPU compute timestamp 差（cull.comp）
    struct FrameMetrics
    {
        double   m1_cpu_submit_ms = 0.0;  // CPU record+submit 段（chrono，已剔除 cull）
        double   m2_gpu_frame_ms  = 0.0;  // GPU 帧时间戳差（top→bottom of pipe）
        uint32_t m3_draw_calls    = 0u;   // host 端 draw 调用次数（路径相关）
        double   m4_cull_ms       = 0.0;  // 剔除耗时（CPU 路径=host chrono；GpuDriven=GPU 时间戳）
        // 论文 chap05 §5.5.4 CULL-RATE 表：本帧实际可见 instance 总数。
        //   - NaiveCpu / CpuIndirect：cullOnCpu 内累加 visible_ids.size()
        //   - GpuDriven：保持 0（避免每帧 GPU→CPU readback 污染 M1；论文表用同 scene
        //                的 CpuIndirect 数据填充 GpuDriven 行，剔除算法相同）
        //   - --disable-cull 模式下：恒等填充 → 本字段 = total_instance_count
        uint32_t m4_visible_instances = 0u;
    };

    // ----- 与 GLSL 端 bindless_structs.glsl 二进制对位的 host 端结构体 -----
    // 以下三个结构体的字段顺序和大小必须严格对应 GLSL std430 布局；
    // 其原型来源是 RenderEngine/src/Vulkan/VulkanRenderer.cpp 中的私有 struct 定义。

    // 与 GLSL camera_uniform.glsl 中 camera_uniform UBO 一一对应。
    struct CameraData
    {
        Matrix4x4<float>  m_projection;
        Matrix4x4<float>  m_view;
        Matrix4x4<float>  m_projection_view;
        Vector4D<float>   m_position;
        Frustum<float>    m_frustum;
    };

    // 与 GLSL bindless_structs.glsl 的 ObjectInfo 对位（顺序：vb / ib / params / texture_ids）。
    // GLSL 端字段类型为 uint64_t，对应 C++ 端 vk::DeviceAddress。
    struct ObjectData
    {
        vk::DeviceAddress m_vb_address                 = 0;
        vk::DeviceAddress m_ib_address                 = 0;
        vk::DeviceAddress m_material_params_address    = 0;
        vk::DeviceAddress m_material_texture_ids_address = 0;
    };

    // 与 GLSL bindless_structs.glsl 的 InstanceInfo 对位。
    struct InstanceData
    {
        Matrix4x4<float>  m_transform {};
    };

    // 与 GLSL vertex_buffer_test.vert 中 DrawMetaInfo 对位。
    // 前 4 字段直接复用 vk::DrawIndirectCommand 的内存布局。
    struct DrawMetaInfo
    {
        uint32_t m_vertex_count   = 0u;
        uint32_t m_instance_count = 0u;
        uint32_t m_first_vertex   = 0u;
        uint32_t m_first_instance = 0u;
        uint32_t m_object_id      = 0u;
    };

    // ----- 字符串化辅助（用于 CSV / 日志输出，禁止重命名以免破坏论文对位）-----

    constexpr std::string_view to_csv_name(ePath path) noexcept
    {
        switch (path) {
            case ePath::eCpuDrivenNaive:    return "eCpuDrivenNaive";
            case ePath::eCpuDrivenIndirect: return "eCpuDrivenIndirect";
            case ePath::eGpuDriven:         return "eGpuDriven";
        }
        return "eUnknown";
    }

    constexpr std::string_view to_csv_name(eScene scene) noexcept
    {
        switch (scene) {
            case eScene::eA: return "A";
            case eScene::eB: return "B";
            case eScene::eC: return "C";
            case eScene::eD: return "D";
        }
        return "X";
    }

    // mode → CSV 短名（与 CLI --modes 参数解析一一对应；不要随意改字面量）。
    constexpr std::string_view to_csv_name(eEmulationMode mode) noexcept
    {
        switch (mode) {
            case eEmulationMode::eClean:            return "clean";
            case eEmulationMode::eLegacy:           return "legacy";
            case eEmulationMode::eSingle:           return "single";
            case eEmulationMode::eGpuDriven:        return "gpu_driven";
            case eEmulationMode::eGpuIndirectCount: return "gpu_indirect_count";
        }
        return "unknown";
    }

}  // namespace lcf::benchmark
