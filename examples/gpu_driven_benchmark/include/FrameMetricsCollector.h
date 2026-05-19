#pragma once

// FrameMetricsCollector：跑分模式下的逐帧指标聚合与 CSV 写入。
//
// 用法约定：
//   1) 每个 (path, scene) 单元开始时调用 beginRun(path, scene, instance_count)；
//   2) 每帧调用 push(FrameMetrics)，逐帧累积；
//   3) 单元跑完调用 endRunAndAppendCsv(out_path)：
//        - 计算 m1/m2/m3/m4 的算术均值；
//        - 在 m2 序列上计算 p99（M5）；
//        - 以追加模式写入 CSV，列与 chap05 §5.4.2 表头一一对应。
//   4) 所有跑分结束后调用 close() 释放文件。
//
// CSV 表头由 ensureHeader 在首次 endRunAndAppendCsv 时写入。
// 一行格式：
//   path,scene,instance_count,m1_cpu_submit_ms,m2_gpu_frame_ms,m3_draw_calls,m4_cull_ms,m5_p99_ms
//
// M4_cull_ms 不区分 CPU/GPU 路径：
//   - NaiveCpu / CpuIndirect：host chrono(cullOnCpu)
//   - GpuDriven：GPU compute timestamp 差（cull.comp）

#include "benchmark_types.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lcf::benchmark {

    struct AggregatedMetrics
    {
        double   m1_cpu_submit_ms_mean = 0.0;
        double   m2_gpu_frame_ms_mean  = 0.0;
        double   m3_draw_calls_mean    = 0.0;  // 用 double 因为 mean 可能非整
        double   m4_cull_ms_mean       = 0.0;
        double   m5_p99_ms             = 0.0;
        uint32_t sample_count          = 0;
    };

    class FrameMetricsCollector
    {
    public:
        FrameMetricsCollector() = default;
        ~FrameMetricsCollector() = default;

        FrameMetricsCollector(const FrameMetricsCollector &) = delete;
        FrameMetricsCollector & operator=(const FrameMetricsCollector &) = delete;

        // 开始一个跑分单元。warmup_frames 仅作为 reserve hint，不在内部丢弃；
        // 调用方应保证 warmup 阶段不调用 push。
        void beginRun(ePath path, eScene scene, uint32_t instance_count, uint32_t expected_samples);

        // 累积一帧指标。
        void push(const FrameMetrics & metrics);

        // 结束当前跑分单元，把均值/p99 行追加到 csv_path（首次调用会写入表头）。
        // 若 csv_path 父目录不存在，自动创建。返回聚合后的指标用于打印。
        AggregatedMetrics endRunAndAppendCsv(const std::filesystem::path & csv_path);

        // 重置所有内部状态（不影响 CSV 文件）。
        void reset() noexcept;

        // 给单元自检使用：纯数学函数，对一个 m2 序列计算 p99（不在内部状态中）。
        // 同时也是 endRunAndAppendCsv 内部使用的实现，提取出来便于做 mock-data 测试。
        static double computeP99Ms(std::vector<double> m2_series);

        // 给单元自检使用：计算均值。
        static double computeMean(const std::vector<double> & values);

    private:
        ePath  m_current_path  = ePath::eCpuDrivenNaive;
        eScene m_current_scene = eScene::eA;
        uint32_t m_instance_count = 0;
        std::vector<FrameMetrics> m_frames;
        bool m_run_active = false;
    };

}  // namespace lcf::benchmark
