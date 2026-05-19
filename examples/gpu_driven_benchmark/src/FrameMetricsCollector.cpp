#include "FrameMetricsCollector.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>

#include "log.h"

namespace fs = std::filesystem;

namespace lcf::benchmark {

    void FrameMetricsCollector::beginRun(
        ePath path, eScene scene, uint32_t instance_count, uint32_t expected_samples)
    {
        m_current_path     = path;
        m_current_scene    = scene;
        m_instance_count   = instance_count;
        m_run_active       = true;
        m_frames.clear();
        m_frames.reserve(expected_samples);
    }

    void FrameMetricsCollector::push(const FrameMetrics & metrics)
    {
        if (not m_run_active) {
            // 调用顺序错误时仍接受，避免吞掉数据；只是日志一条 warn。
            lcf_log_warn("FrameMetricsCollector::push called without active run");
            m_run_active = true;
        }
        m_frames.emplace_back(metrics);
    }

    double FrameMetricsCollector::computeMean(const std::vector<double> & values)
    {
        if (values.empty()) { return 0.0; }
        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        return sum / static_cast<double>(values.size());
    }

    double FrameMetricsCollector::computeP99Ms(std::vector<double> m2_series)
    {
        if (m2_series.empty()) { return 0.0; }
        // p99 索引：取 ceil(0.99 * n) - 1，限制到 [0, n-1]。
        const size_t n = m2_series.size();
        size_t idx = static_cast<size_t>(std::ceil(0.99 * static_cast<double>(n)));
        if (idx == 0) { idx = 1; }
        if (idx > n) { idx = n; }
        const size_t k = idx - 1;
        std::nth_element(m2_series.begin(), m2_series.begin() + k, m2_series.end());
        return m2_series[k];
    }

    AggregatedMetrics FrameMetricsCollector::endRunAndAppendCsv(const fs::path & csv_path)
    {
        AggregatedMetrics agg;
        if (m_frames.empty()) {
            lcf_log_warn("FrameMetricsCollector::endRunAndAppendCsv: empty samples for path={} scene={}",
                         to_csv_name(m_current_path), to_csv_name(m_current_scene));
            m_run_active = false;
            return agg;
        }

        // 拆 5 个序列做聚合。
        std::vector<double> m1, m2, m3, m4;
        m1.reserve(m_frames.size());
        m2.reserve(m_frames.size());
        m3.reserve(m_frames.size());
        m4.reserve(m_frames.size());
        for (const auto & f : m_frames) {
            m1.emplace_back(f.m1_cpu_submit_ms);
            m2.emplace_back(f.m2_gpu_frame_ms);
            m3.emplace_back(static_cast<double>(f.m3_draw_calls));
            m4.emplace_back(f.m4_cull_ms);
        }

        agg.m1_cpu_submit_ms_mean = computeMean(m1);
        agg.m2_gpu_frame_ms_mean  = computeMean(m2);
        agg.m3_draw_calls_mean    = computeMean(m3);
        agg.m4_cull_ms_mean       = computeMean(m4);
        agg.m5_p99_ms             = computeP99Ms(m2);  // 复制副本进入；nth_element 会修改
        agg.sample_count          = static_cast<uint32_t>(m_frames.size());

        // 确保父目录存在。
        fs::create_directories(csv_path.parent_path());

        const bool need_header = not fs::exists(csv_path);
        std::ofstream ofs(csv_path, std::ios::app);
        if (not ofs) {
            lcf_log_error("FrameMetricsCollector: failed to open csv: {}", csv_path.string());
            m_run_active = false;
            return agg;
        }
        if (need_header) {
            ofs << "path,scene,instance_count,m1_cpu_submit_ms,m2_gpu_frame_ms,"
                << "m3_draw_calls,m4_cull_ms,m5_p99_ms,sample_count\n";
        }
        ofs << to_csv_name(m_current_path) << ','
            << to_csv_name(m_current_scene) << ','
            << m_instance_count << ','
            << agg.m1_cpu_submit_ms_mean << ','
            << agg.m2_gpu_frame_ms_mean << ','
            << agg.m3_draw_calls_mean << ','
            << agg.m4_cull_ms_mean << ','
            << agg.m5_p99_ms << ','
            << agg.sample_count << '\n';
        ofs.flush();

        lcf_log_info(
            "metrics[{}/{}]: M1={:.4f}ms M2={:.4f}ms M3={:.1f} M4_cull={:.4f}ms p99={:.4f}ms n={}",
            to_csv_name(m_current_path), to_csv_name(m_current_scene),
            agg.m1_cpu_submit_ms_mean, agg.m2_gpu_frame_ms_mean,
            agg.m3_draw_calls_mean, agg.m4_cull_ms_mean,
            agg.m5_p99_ms, agg.sample_count);

        m_run_active = false;
        return agg;
    }

    void FrameMetricsCollector::reset() noexcept
    {
        m_frames.clear();
        m_run_active = false;
    }

}  // namespace lcf::benchmark
