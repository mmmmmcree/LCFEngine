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
    struct FrameMetrics
    {
        double   m1_cpu_submit_ms = 0.0;  // CPU record + submit 段（chrono）
        double   m2_gpu_frame_ms  = 0.0;  // GPU 帧时间戳差（top→bottom of pipe）
        uint32_t m3_draw_calls    = 0u;   // host 端 draw 调用次数（路径相关）
        double   m4_gpu_cull_ms   = 0.0;  // GPU 视锥剔除耗时（仅 GpuDriven 非零）
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

}  // namespace lcf::benchmark
