// gpu_driven_benchmark 入口。
//
// 两种运行模式：
//   1) 默认交互模式：弹窗显示，TrackballController 控制相机；
//      数字键 1/2/3 切换路径（Naive / CpuIndirect / GpuDriven），
//      F1/F2/F3/F4 切换场景档位（A/B/C/D），HUD 输出当前指标。
//   2) `--benchmark`：headless 跑分，遍历 (path × scene) 笛卡尔积，
//      每格预热 200 帧 + 采样 1000 帧，结果写入
//      `<bin>/<Config>/benchmark_results/result_YYYYMMDD_HHMMSS.csv`。
//
// 入口骨架沿用 `examples/main_example/src/main.cpp` 的双调度器架构：
//   引擎主循环走 1ms 渲染 PeriodicTask；窗口走 5ms pollEvents PeriodicTask。

#include "BenchmarkScene.h"
#include "FrameMetricsCollector.h"
#include "NaiveCpuRenderer.h"
#include "CpuIndirectRenderer.h"
#include "RendererSwitcher.h"
#include "benchmark_types.h"
#include "VkDebugLabel.h"

#include "ResourceSystem.h"
#include "TrackballController.h"
#include "Transform.h"
#include "TransformSystem.h"
#include "Vulkan/VulkanContext.h"
#include "ecs/signals.h"
#include "gui/gui_types.h"
#include "input_types.h"
#include "log.h"
#include "tasks/TaskScheduler.h"
#include "tracy_profiling.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <future>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {
    using lcf::benchmark::ePath;
    using lcf::benchmark::eEmulationMode;
    using lcf::benchmark::eScene;

    // (path, mode) 笛卡尔积矩阵 —— 跑分模式默认遍历全 5 组合。
    struct PathModePair { ePath path; eEmulationMode mode; };
    const PathModePair k_full_matrix[] = {
        {ePath::eCpuDrivenNaive,    eEmulationMode::eClean},
        {ePath::eCpuDrivenNaive,    eEmulationMode::eLegacy},
        {ePath::eCpuDrivenIndirect, eEmulationMode::eSingle},
        {ePath::eCpuDrivenIndirect, eEmulationMode::eLegacy},
        {ePath::eGpuDriven,         eEmulationMode::eGpuDriven},
    };
    constexpr size_t k_full_matrix_count = sizeof(k_full_matrix) / sizeof(k_full_matrix[0]);

    // ------------- CLI -------------
    struct CliArgs
    {
        bool show_help    = false;
        bool show_version = false;
        bool self_test    = false;
        bool benchmark    = false;
        bool ablation     = false;  // step6：场景 C 4 行消融独立 CSV
        bool camera_stress = false; // step7：相机斜视外角，让 B/C/D 档剔除率有意义（CULL-RATE 表）
        // 跑分参数（可选 override）
        uint32_t warmup_frames  = 200;
        uint32_t sample_frames  = 1000;
        // 默认输出目录：<exe>/benchmark_results/result_YYYYMMDD_HHMMSS.csv
        std::filesystem::path out_csv;
        // 输出目录（step7）：CSV / .tracy / zone_csv 集中放这里。空 = 走默认 benchmark_results
        std::filesystem::path out_dir;
        // step5 新增：
        // - modes_filter：CSV 短名集合（clean/legacy/single/gpu_driven/gpu_indirect_count）；空 = 全跑
        //   注：legacy 同时匹配 NaiveCpu_legacy 与 CpuIndirect_legacy（两路径共享同一 mode 短名）
        // - pipeline_switch_period：NaiveCpu_legacy 每 N 实例 toggle 一次 PSO（默认 4096）
        std::unordered_set<std::string> modes_filter;
        uint32_t pipeline_switch_period = 4096u;
        // step6：交互模式下手动开关 disable_cull（仅做单次实验时方便观察）
        bool disable_cull = false;
        // step7：tracy-capture 抓取（要求 tracy-capture 在 PATH）
        bool tracy_capture_per_unit = false;
    };

    CliArgs parse_cli(int argc, char * argv[])
    {
        CliArgs args;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--help" || a == "-h") { args.show_help = true; }
            else if (a == "--version") { args.show_version = true; }
            else if (a == "--self-test") { args.self_test = true; }
            else if (a == "--benchmark") { args.benchmark = true; }
            else if (a == "--ablation")  { args.ablation  = true; }
            else if (a == "--camera-stress") { args.camera_stress = true; }
            else if (a == "--tracy-capture") { args.tracy_capture_per_unit = true; }
            else if (a == "--out-dir" && i + 1 < argc) { args.out_dir = argv[++i]; }
            else if (a == "--disable-cull") { args.disable_cull = true; }
            else if (a == "--warmup" && i + 1 < argc) {
                args.warmup_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
            else if (a == "--samples" && i + 1 < argc) {
                args.sample_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
            else if (a == "--out" && i + 1 < argc) {
                args.out_csv = argv[++i];
            }
            else if (a == "--modes" && i + 1 < argc) {
                // 逗号分隔，如 "clean,legacy,gpu_driven"
                std::string list = argv[++i];
                size_t pos = 0;
                while (pos < list.size()) {
                    size_t comma = list.find(',', pos);
                    std::string token = list.substr(pos, comma - pos);
                    if (not token.empty()) { args.modes_filter.insert(token); }
                    if (comma == std::string::npos) { break; }
                    pos = comma + 1;
                }
            }
            else if (a == "--pipeline-switch-period" && i + 1 < argc) {
                args.pipeline_switch_period = static_cast<uint32_t>(std::stoul(argv[++i]));
                if (args.pipeline_switch_period == 0u) { args.pipeline_switch_period = 1u; }
            }
        }
        return args;
    }

    void print_help()
    {
        std::puts("gpu_driven_benchmark (LCFEngine experiment)");
        std::puts("Usage:");
        std::puts("  gpu_driven_benchmark                       interactive mode (default)");
        std::puts("    keys: 1/2/3  -> switch path (Naive / CpuIndirect / GpuDriven)");
        std::puts("    keys: M      -> cycle emulation mode within active path");
        std::puts("                    Naive:       clean <-> legacy");
        std::puts("                    CpuIndirect: single <-> legacy");
        std::puts("                    GpuDriven:   gpu_driven <-> gpu_indirect_count");
        std::puts("    keys: 4/5/6/7 -> switch scene scale (A/B/C/D)");
        std::puts("  gpu_driven_benchmark --benchmark           headless main matrix (5 modes x 4 scenes)");
        std::puts("  gpu_driven_benchmark --ablation            headless ablation (scene C, 4 rows)");
        std::puts("    --warmup N         (default 200)");
        std::puts("    --samples N        (default 1000)");
        std::puts("    --out <path.csv>   (default benchmark_results/result_TS.csv or ablation_TS.csv)");
        std::puts("    --modes a,b,c,...  (default all 5; Naive_legacy/Indirect_legacy 共享 'legacy')");
        std::puts("    --pipeline-switch-period N  (default 4096; only affects NaiveCpu_legacy)");
        std::puts("    --camera-stress    (skewed camera @ grid corner, ~30/60% culled for B/C/D)");
        std::puts("    --tracy-capture    (spawn tracy-capture per unit; requires tracy-capture in PATH)");
        std::puts("    --out-dir <dir>    (override output directory; csv/.tracy/zone_csv all go here)");
        std::puts("  gpu_driven_benchmark --self-test           run unit self-tests");
    }

    void print_version()
    {
        std::puts("gpu_driven_benchmark v0.1.0");
    }

    // self-test 仅测 collector 数学（与上一阶段相同）。
    int run_self_tests()
    {
        using lcf::benchmark::FrameMetricsCollector;
        const std::vector<double> v{1.0, 2.0, 3.0, 4.0, 5.0};
        if (std::abs(FrameMetricsCollector::computeMean(v) - 3.0) > 1e-9) { return 1; }
        std::vector<double> v100;
        for (int i = 1; i <= 100; ++i) { v100.emplace_back(static_cast<double>(i)); }
        if (std::abs(FrameMetricsCollector::computeP99Ms(v100) - 99.0) > 1e-9) { return 1; }
        std::puts("[self-test] OK");
        return 0;
    }

    std::string make_timestamp_suffix()
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
#if defined(_MSC_VER)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    // 把 (path, mode) 对应到具体 renderer 实例并设置 mode。
    // GpuDriven 接受 eGpuDriven / eGpuIndirectCount；其它 renderer 各自接受对应集合。
    void apply_mode(lcf::benchmark::RendererSwitcher & switcher, ePath path, eEmulationMode mode)
    {
        switcher.switchPath(path);
        if (auto * r = switcher.getRenderer(path)) {
            r->setEmulationMode(mode);
        }
    }

    // 设置当前 active path 的 disable_cull（消融跑分用）。
    void apply_disable_cull(lcf::benchmark::RendererSwitcher & switcher, ePath path, bool disabled)
    {
        if (auto * r = switcher.getRenderer(path)) {
            r->setDisableCull(disabled);
        }
    }

    // 相机姿态：默认 vs camera-stress（让剔除真实生效）。
    //   - 默认：(0, 0, 50)，看 -Z，FOV 60° 包住 grid → 剔除率 ~0%
    //   - stress：(35, 0, 35) + 绕 Y 轴 -45°，斜视 grid 一角 → 剔除率 60-80%（依 scene 而异）
    // 实现：先 reset 到原点，再 transform 到目标姿态。
    void apply_camera_pose(lcf::Transform & camera_transform,
                           lcf::ecs::Registry & registry,
                           const lcf::ecs::Entity & camera_entity,
                           lcf::ecs::TransformSystem & transform_system,
                           bool stress)
    {
        // Reset 到单位矩阵；下面再 translate / rotate 到目标姿态。
        lcf::Matrix4x4<float> id;
        id.setToIdentity();
        camera_transform.setLocalMatrix(id);
        if (stress) {
            // grid 范围 ±22.5（D 档）。把相机放到 (35, 0, 35)，绕 Y 轴 -45°（朝 -X-Z 看 grid 中心）。
            camera_transform.translateWorld(35.0f, 0.0f, 35.0f);
            camera_transform.rotateLocalYAxis(-45.0f);
        } else {
            camera_transform.translateWorld(0.0f, 0.0f, 50.0f);
        }
        registry.enqueueSignal<lcf::ecs::TransformUpdateSignal>({camera_entity.getId()});
        registry.ctx().get<lcf::ecs::Dispatcher>().update();
        transform_system.update();
    }

    // 启动 tracy-capture 子进程（要求 tracy-capture 在 PATH）。
    // 返回 future：调用方在 sample 段跑完后 .wait() 即可。
    // - tracy-capture 是阻塞型 CLI：默认连不到 server 时会一直 retry，连上之后开始录，
    //   直到 -s 秒数到点才主动 disconnect 并落盘退出
    // - 用更长的 capture 时长（sample 实际时长 + 安全余量）保证不被截断
    std::future<int> spawn_tracy_capture(const std::filesystem::path & tracy_file, double seconds)
    {
        return std::async(std::launch::async, [tracy_file, seconds]() -> int {
            // -f 强制覆盖；-s 持续秒数（向上取整 + 余量）；-o 输出文件
            const int total_s = static_cast<int>(std::ceil(seconds));
            std::string cmd = "tracy-capture -f -s " + std::to_string(total_s) +
                              " -o \"" + tracy_file.string() + "\" >NUL 2>&1";
            return std::system(cmd.c_str());
        });
    }

    // 跑分结束后一次性把 out_dir 内所有 .tracy 转成 <stem>_zones.csv（zone 统计：mean/min/max/std）。
    // 这是 chap05 §5.5.6 \"Tracy CPU 端时间分布\"表的数据来源。
    // 要求 tracy-csvexport 在 PATH 里。
    int export_all_tracy_zones(const std::filesystem::path & out_dir)
    {
        if (out_dir.empty() or not std::filesystem::is_directory(out_dir)) { return 0; }
        int success = 0;
        int failed  = 0;
        for (auto & entry : std::filesystem::directory_iterator(out_dir)) {
            if (entry.path().extension() != ".tracy") { continue; }
            auto out_csv = entry.path();
            out_csv.replace_extension();  // strip .tracy
            out_csv += "_zones.csv";
            std::string cmd = "tracy-csvexport \"" + entry.path().string() +
                              "\" > \"" + out_csv.string() + "\" 2>NUL";
            int rc = std::system(cmd.c_str());
            if (rc == 0) {
                ++success;
                lcf_log_info("[zone-csv] {} -> {}", entry.path().filename().string(),
                             out_csv.filename().string());
            } else {
                ++failed;
                lcf_log_warn("[zone-csv] FAILED rc={} for {}", rc, entry.path().string());
            }
        }
        lcf_log_info("[zone-csv] total: {} ok / {} failed", success, failed);
        return failed;
    }

    // 给 NaiveCpu 配置 pipeline_switch_period（仅 eLegacy 起效）。
    void apply_pipeline_switch_period(lcf::benchmark::RendererSwitcher & switcher, uint32_t period)
    {
        if (auto * r = switcher.getRenderer(ePath::eCpuDrivenNaive)) {
            // setEmulationMode 是 IBenchmarkRenderer 接口；setPipelineSwitchPeriod 是 NaiveCpu 专属
            // 这里向下转一次（只有 NaiveCpuRenderer 才有该方法）。
            if (auto * naive = dynamic_cast<lcf::benchmark::NaiveCpuRenderer *>(r)) {
                naive->setPipelineSwitchPeriod(period);
            }
        }
    }
}

// ---------------- main ----------------
int main(int argc, char * argv[])
{
    lcf::log::init();
    lcf_log_info("[bench] main start");

    auto args = parse_cli(argc, argv);
    if (args.show_help) { print_help(); return 0; }
    if (args.show_version) { print_version(); return 0; }
    if (args.self_test) { return run_self_tests(); }

    // ---- 引擎初始化（与 main_example 完全同形）----
    lcf_log_info("[bench] before Registry");
    lcf::ecs::Registry registry{{
        lcf::TaskSchedulerCreateInfo{lcf::TaskSchedulerRunMode::eNewThread}
    }};
    lcf_log_info("[bench] after Registry, before VulkanContext");
    lcf::render::VulkanContext context;

    lcf_log_info("[bench] before WindowSystem.allocateWindow");
    auto window_up = lcf::gui::WindowSystem::getInstance().allocateWindow();
    lcf::gui::WindowCreateInfo window_info;
    window_info.setRegistryPtr(&registry)
        .setWidth(1280)
        .setHeight(720)
        .setSurfaceType(lcf::gui::SurfaceType::eVulkan);
    window_up->create(window_info);
    window_up->show();
    lcf_log_info("[bench] window created+shown");

    context.registerWindow(window_up->getEntity()).create();
    lcf_log_info("[bench] vulkan context created");
    registry.ctx().emplace<lcf::ecs::ResourceSystem>(registry);
    lcf_log_info("[bench] resource system installed");

    // ---- 实验对象初始化 ----
    lcf_log_info("[bench] before scene.create");
    lcf::benchmark::BenchmarkScene scene;
    scene.create(&context, registry);
    lcf_log_info("[bench] after scene.create");

    // 初始化 VK_EXT_debug_utils 函数指针（VkDebugLabel 内 lazy 用）。
    // 给 nsys / RenderDoc 在 cmd buffer timeline 上识别 GpuDriven::CullDispatch
    // 等关键段，作为 query pool M2/M4 的双源认证（chap05 §5.5.6）。
    lcf::benchmark::VkDebugLabel::init(context.getDevice());


    // 先把场景规模摆到默认档（eA），再创建 renderer。这样 RendererSwitcher 创建
    // GpuDrivenRenderer 时 scene 的 instance_data / bounding / draw_meta 已就绪，
    // 不会出现 mesh_count = 0 触发的 div-by-zero。
    scene.setSceneScale(lcf::benchmark::eScene::eA);
    lcf_log_info("[bench] after scene.setSceneScale eA, total_instances={}",
                 scene.getTotalInstanceCount());

    lcf::benchmark::RendererSwitcher switcher;
    switcher.create(&context, &scene,
                    lcf::gui::WindowSystem::getInstance()
                        .getPrimaryDisplayerInfo().getDesktopModeInfo().getRenderExtent(),
                    registry);
    lcf_log_info("[bench] after switcher.create");

    // 默认从 GpuDriven 路径起步，便于第一帧就看到完整画面。
    switcher.switchPath(lcf::benchmark::ePath::eGpuDriven);
    lcf_log_info("[bench] after switchPath eGpuDriven");

    // ---- camera + trackball ----
    lcf::ecs::TransformSystem transform_system(registry);
    lcf::ecs::Entity camera_entity(registry);
    auto & camera_transform = camera_entity.requireComponent<lcf::Transform>();
    camera_entity.requireComponent<lcf::TransformInvertedWorldMatrix>(camera_transform.getWorldMatrix());

    lcf::InputReader input_reader;
    lcf::modules::TrackballController trackball;
    trackball.setSensitivity(80.0f).setMoveSpeed(50.0f).setZoomSpeed(400.0f)
             .setInputReader(input_reader);

    // ---- 跑分模式：headless 自驱动 ----
    if (args.benchmark) {
        // 跑分模式仍需要 window（用于 swapchain），但不在主线程做 trackball；
        // 相机用一组固定姿态使每个 (path, mode, scene) 跑分时画面一致。

        // out_dir 优先级 > out_csv：若给了 out_dir，CSV 落在 out_dir/main.csv，.tracy 也落在该目录。
        std::filesystem::path out_dir = args.out_dir;
        std::filesystem::path csv_path = args.out_csv;
        if (out_dir.empty() and csv_path.empty()) {
            const std::string suffix = "result_" + make_timestamp_suffix() +
                                       (args.camera_stress ? "_stress" : "");
            out_dir = std::filesystem::path("benchmark_results") / suffix;
        }
        if (csv_path.empty()) {
            csv_path = out_dir.empty()
                        ? std::filesystem::path("benchmark_results") /
                          ("result_" + make_timestamp_suffix() + ".csv")
                        : (out_dir / "main.csv");
        }
        std::filesystem::create_directories(csv_path.parent_path());
        lcf_log_info("benchmark mode: warmup={} samples={} out={} switch_period={} camera_stress={} tracy={}",
                     args.warmup_frames, args.sample_frames, csv_path.string(),
                     args.pipeline_switch_period,
                     args.camera_stress, args.tracy_capture_per_unit);

        lcf::benchmark::FrameMetricsCollector collector;

        const lcf::benchmark::eScene scenes[] = {
            lcf::benchmark::eScene::eA,
            lcf::benchmark::eScene::eB,
            lcf::benchmark::eScene::eC,
            lcf::benchmark::eScene::eD,
        };

        apply_camera_pose(camera_transform, registry, camera_entity, transform_system,
                          args.camera_stress);

        for (size_t mi = 0; mi < k_full_matrix_count; ++mi) {
            const auto & pm = k_full_matrix[mi];
            // --modes 过滤
            if (not args.modes_filter.empty()) {
                std::string short_name(lcf::benchmark::to_csv_name(pm.mode));
                if (args.modes_filter.find(short_name) == args.modes_filter.end()) {
                    continue;
                }
            }
            apply_mode(switcher, pm.path, pm.mode);
            apply_pipeline_switch_period(switcher, args.pipeline_switch_period);
            apply_disable_cull(switcher, pm.path, false);  // 主表恒为 false

            for (auto sc : scenes) {
                scene.setSceneScale(sc);
                const uint32_t inst = scene.getTotalInstanceCount();

                // warmup
                for (uint32_t i = 0; i < args.warmup_frames; ++i) {
                    window_up->pollEvents();
                    if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto done; }
                    switcher.render(camera_entity, window_up->getEntity());
                }

                // 启动 tracy-capture 子进程（如启用），估算 capture 时长 = sample_frames / 估算 FPS。
                // 用 warmup 段最后 N 帧的实际帧时间反推 FPS；初值给 200 FPS（Release 兜底）。
                std::future<int> tracy_future;
                if (args.tracy_capture_per_unit and not out_dir.empty()) {
                    // 文件名：<path>_<mode>_<scene>.tracy
                    std::string fname = std::string(lcf::benchmark::to_csv_name(pm.path)) + "_" +
                                        std::string(lcf::benchmark::to_csv_name(pm.mode)) + "_" +
                                        std::string(lcf::benchmark::to_csv_name(sc)) + ".tracy";
                    auto tracy_file = out_dir / fname;
                    // capture 时长：保守按 30 FPS 估 sample 段 + 5s 安全余量。capture 会提前
                    // 录满 sample 段，剩余时间 client idle 也能继续维持 connection。
                    const double cap_seconds = static_cast<double>(args.sample_frames) / 30.0 + 5.0;
                    tracy_future = spawn_tracy_capture(tracy_file, cap_seconds);
                    // 等 capture 与 client 完整 handshake（300ms 比 150ms 更稳，避免连接竞速）
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    lcf_log_info("[tracy] spawn capture -> {} (cap={:.1f}s)",
                                 tracy_file.string(), cap_seconds);
                }

                // sample
                collector.beginRun(pm.path, pm.mode, sc, inst, args.sample_frames, /*disable_cull=*/false);
                for (uint32_t i = 0; i < args.sample_frames; ++i) {
                    window_up->pollEvents();
                    if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto done; }
                    auto m = switcher.render(camera_entity, window_up->getEntity());
                    collector.push(m);
                    LCF_TRACY_FRAMEMARK;
                }
                collector.endRunAndAppendCsv(csv_path);

                // 等待 tracy-capture 落盘（如启用）。tracy-capture 自己 -s 计时到点退出。
                if (tracy_future.valid()) {
                    lcf_log_info("[tracy] waiting capture to flush...");
                    int rc = tracy_future.get();
                    lcf_log_info("[tracy] capture done rc={}", rc);
                    // 给 TracyClient 内部 finalize + 准备接受下一次 connect 留充足时间，
                    // 避免相邻两次 capture 之间出现 reconnect race 导致 .tracy 损坏。
                    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                }
            }
        }
done:
        context.getDevice().waitIdle();
        if (args.tracy_capture_per_unit and not out_dir.empty()) {
            export_all_tracy_zones(out_dir);
        }
        lcf_log_info("benchmark done. csv: {}", csv_path.string());
        return 0;
    }


    // ---- 消融模式：场景 C 4 行（ABL-FULL / ABL-CULL / ABL-DGC / ABL-NONE）----
    // 与论文 chap05 §5.5.2 表 5-x 对位。CSV 路径默认 ablation_TS.csv（与主跑分独立）。
    // 跑分单元：
    //   1) gpu_driven        + cull on   = 完整 GPU-Driven 基准（ABL-FULL）
    //   2) gpu_driven        + cull off  = 关闭剔除（ABL-CULL）
    //   3) gpu_indirect_count + cull on  = 关闭 DGC（ABL-DGC）
    //   4) gpu_indirect_count + cull off = 同时关闭剔除与 DGC（叠加，作为参照）
    if (args.ablation) {
        std::filesystem::path out_dir = args.out_dir;
        std::filesystem::path csv_path = args.out_csv;
        if (out_dir.empty() and csv_path.empty()) {
            out_dir = std::filesystem::path("benchmark_results") /
                      ("ablation_" + make_timestamp_suffix());
        }
        if (csv_path.empty()) {
            csv_path = out_dir.empty()
                        ? std::filesystem::path("benchmark_results") /
                          ("ablation_" + make_timestamp_suffix() + ".csv")
                        : (out_dir / "ablation.csv");
        }
        std::filesystem::create_directories(csv_path.parent_path());
        lcf_log_info("ablation mode: warmup={} samples={} out={} tracy={}",
                     args.warmup_frames, args.sample_frames, csv_path.string(),
                     args.tracy_capture_per_unit);

        lcf::benchmark::FrameMetricsCollector collector;

        apply_camera_pose(camera_transform, registry, camera_entity, transform_system,
                          args.camera_stress);

        struct AblRow { eEmulationMode mode; bool disable_cull; const char * tag; };
        const AblRow rows[] = {
            { eEmulationMode::eGpuDriven,        false, "full"          },
            { eEmulationMode::eGpuDriven,        true,  "no_cull"       },
            { eEmulationMode::eGpuIndirectCount, false, "no_dgc"        },
            { eEmulationMode::eGpuIndirectCount, true,  "no_dgc_no_cull"},
        };

        const auto sc = lcf::benchmark::eScene::eC;
        scene.setSceneScale(sc);
        const uint32_t inst = scene.getTotalInstanceCount();

        for (const auto & row : rows) {
            apply_mode(switcher, ePath::eGpuDriven, row.mode);
            apply_disable_cull(switcher, ePath::eGpuDriven, row.disable_cull);

            // warmup
            for (uint32_t i = 0; i < args.warmup_frames; ++i) {
                window_up->pollEvents();
                if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto abl_done; }
                switcher.render(camera_entity, window_up->getEntity());
            }

            std::future<int> tracy_future;
            if (args.tracy_capture_per_unit and not out_dir.empty()) {
                std::string fname = std::string("abl_") + row.tag + ".tracy";
                auto tracy_file = out_dir / fname;
                const double cap_seconds = static_cast<double>(args.sample_frames) / 30.0 + 5.0;
                tracy_future = spawn_tracy_capture(tracy_file, cap_seconds);
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                lcf_log_info("[tracy] spawn capture -> {} (cap={:.1f}s)",
                             tracy_file.string(), cap_seconds);
            }

            // sample
            collector.beginRun(ePath::eGpuDriven, row.mode, sc, inst, args.sample_frames, row.disable_cull);
            for (uint32_t i = 0; i < args.sample_frames; ++i) {
                window_up->pollEvents();
                if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto abl_done; }
                auto m = switcher.render(camera_entity, window_up->getEntity());
                collector.push(m);
                LCF_TRACY_FRAMEMARK;
            }
            collector.endRunAndAppendCsv(csv_path);
            lcf_log_info("[ablation] mode={} disable_cull={} done",
                         lcf::benchmark::to_csv_name(row.mode), row.disable_cull);
            if (tracy_future.valid()) {
                tracy_future.get();
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
        }
abl_done:
        // 跑完恢复 disable_cull = false 以免污染交互模式
        apply_disable_cull(switcher, ePath::eGpuDriven, false);
        context.getDevice().waitIdle();
        if (args.tracy_capture_per_unit and not out_dir.empty()) {
            export_all_tracy_zones(out_dir);
        }
        lcf_log_info("ablation done. csv: {}", csv_path.string());
        return 0;
    }



    // ---- 交互模式：双调度器，按键切路径/场景 ----
    // 路径切换 1/2/3；M 键在当前 path 内 cycle mode；场景切换 4/5/6/7（A/B/C/D）。
    auto current_scene_idx = static_cast<uint32_t>(0);  // A
    std::array<lcf::benchmark::ePath, 3> path_keys = {
        lcf::benchmark::ePath::eCpuDrivenNaive,
        lcf::benchmark::ePath::eCpuDrivenIndirect,
        lcf::benchmark::ePath::eGpuDriven,
    };
    // 每条 path 当前 mode（HUD 显示用 + M 键 cycle 用）。
    // 默认 mode 与 renderer 内部默认值对齐：Naive = clean，Indirect = single。
    std::array<eEmulationMode, 3> path_modes = {
        eEmulationMode::eClean,      // [0] eCpuDrivenNaive
        eEmulationMode::eSingle,     // [1] eCpuDrivenIndirect
        eEmulationMode::eGpuDriven,  // [2] eGpuDriven
    };
    auto cycle_mode = [](ePath p, eEmulationMode cur) -> eEmulationMode {
        switch (p) {
            case ePath::eCpuDrivenNaive:
                return (cur == eEmulationMode::eClean) ? eEmulationMode::eLegacy : eEmulationMode::eClean;
            case ePath::eCpuDrivenIndirect:
                return (cur == eEmulationMode::eSingle) ? eEmulationMode::eLegacy : eEmulationMode::eSingle;
            case ePath::eGpuDriven:
            default:
                return (cur == eEmulationMode::eGpuDriven) ? eEmulationMode::eGpuIndirectCount : eEmulationMode::eGpuDriven;
        }
    };
    auto path_to_idx = [&](ePath p) -> int {
        switch (p) {
            case ePath::eCpuDrivenNaive:    return 0;
            case ePath::eCpuDrivenIndirect: return 1;
            case ePath::eGpuDriven:         return 2;
        }
        return 2;
    };

    // 起手 GpuDriven：renderer 内部默认会被 Switcher 在 lazy create 时建好；
    // 但 NaiveCpu 的 pipeline_switch_period 也要先配置，等 Naive 实例懒创建后再 apply。
    apply_pipeline_switch_period(switcher, args.pipeline_switch_period);

    bool prev_key_state[8] = {false, false, false, false, false, false, false, false};  // 1/2/3/4/5/6/7/M
    auto check_press_edge = [&](lcf::KeyboardKey k, int prev_idx) -> bool {
        const bool now = input_reader.getCurrentState().isKeyPressed(k);
        const bool was = prev_key_state[prev_idx];
        prev_key_state[prev_idx] = now;
        return now and not was;
    };

    // 路径切换在 1ms 渲染 PeriodicTask lambda 内同步执行。
    // 首次切到 Naive/CpuIndirect 时内部会触发 shaderc 编译新 shader（耗时 1-3 秒），
    // 期间渲染线程暂停，窗口看起来"卡死"——这是预期行为，编译完成后会恢复。
    // 二次切换不再触发编译（renderer 实例已 lazy-cached）。

    auto render_loop = [&] {
        TRACY_SCOPE_BEGIN_NC("BenchmarkFrame", tracy::Color::Cyan);
        static auto last_time = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_time).count();
        last_time = now;

        input_reader.update(window_up->getComponent<lcf::InputCollector>().getSnapshot());
        if (trackball.update(camera_transform, delta)) {
            registry.enqueueSignal<lcf::ecs::TransformUpdateSignal>({camera_entity.getId()});
        }

        // 路径切换（数字键 1/2/3）。注意：首次切到某路径时其内部会调用
        // shaderc 编译 GLSL（FlightHelmet 那段渲染会暂停 1-3 秒），并不是死锁。
        if (check_press_edge(lcf::KeyboardKey::e1, 0)) {
            lcf_log_info("[bench] switching to path={} (may pause briefly for shader compile)",
                         lcf::benchmark::to_csv_name(path_keys[0]));
            switcher.switchPath(path_keys[0]);
            apply_pipeline_switch_period(switcher, args.pipeline_switch_period);
            if (auto * r = switcher.getRenderer(path_keys[0])) {
                r->setEmulationMode(path_modes[0]);
            }
        }
        if (check_press_edge(lcf::KeyboardKey::e2, 1)) {
            lcf_log_info("[bench] switching to path={} (may pause briefly for shader compile)",
                         lcf::benchmark::to_csv_name(path_keys[1]));
            switcher.switchPath(path_keys[1]);
            if (auto * r = switcher.getRenderer(path_keys[1])) {
                r->setEmulationMode(path_modes[1]);
            }
        }
        if (check_press_edge(lcf::KeyboardKey::e3, 2)) {
            lcf_log_info("[bench] switching to path={}", lcf::benchmark::to_csv_name(path_keys[2]));
            switcher.switchPath(path_keys[2]);
            if (auto * r = switcher.getRenderer(path_keys[2])) {
                r->setEmulationMode(path_modes[2]);
            }
        }

        // M 键：在当前 active path 内 cycle 模式。
        if (check_press_edge(lcf::KeyboardKey::eM, 7)) {
            const auto active = switcher.getActivePath();
            const int idx = path_to_idx(active);
            const auto next_mode = cycle_mode(active, path_modes[idx]);
            if (next_mode != path_modes[idx]) {
                path_modes[idx] = next_mode;
                if (auto * r = switcher.getRenderer(active)) {
                    r->setEmulationMode(next_mode);
                }
                lcf_log_info("[bench] mode -> {} (path={})",
                             lcf::benchmark::to_csv_name(next_mode),
                             lcf::benchmark::to_csv_name(active));
            }
        }

        // 场景切换（数字键 4/5/6/7 → A/B/C/D；F1-F4 未在 SDL key map 中）。
        if (check_press_edge(lcf::KeyboardKey::e4, 3)) {
            scene.setSceneScale(lcf::benchmark::eScene::eA);
            lcf_log_info("scene = A, total_instances = {}", scene.getTotalInstanceCount());
        }
        if (check_press_edge(lcf::KeyboardKey::e5, 4)) {
            scene.setSceneScale(lcf::benchmark::eScene::eB);
            lcf_log_info("scene = B, total_instances = {}", scene.getTotalInstanceCount());
        }
        if (check_press_edge(lcf::KeyboardKey::e6, 5)) {
            scene.setSceneScale(lcf::benchmark::eScene::eC);
            lcf_log_info("scene = C, total_instances = {}", scene.getTotalInstanceCount());
        }
        if (check_press_edge(lcf::KeyboardKey::e7, 6)) {
            scene.setSceneScale(lcf::benchmark::eScene::eD);
            lcf_log_info("scene = D, total_instances = {}", scene.getTotalInstanceCount());
        }

        registry.ctx().get<lcf::ecs::Dispatcher>().update();
        transform_system.update();

        auto metrics = switcher.render(camera_entity, window_up->getEntity());
        LCF_TRACY_FRAMEMARK;  // 标记 Tracy timeline 帧边界

        // 简易 HUD：每 30 帧打一次指标到日志（不阻塞主循环）。
        static uint32_t hud_counter = 0;
        if ((++hud_counter % 30) == 0) {
            const auto active = switcher.getActivePath();
            const int idx = path_to_idx(active);
            lcf_log_info("path={}/{} scene_inst={} M1={:.3f}ms M2={:.3f}ms M3={} M4_cull={:.3f}ms",
                         lcf::benchmark::to_csv_name(active),
                         lcf::benchmark::to_csv_name(path_modes[idx]),
                         scene.getTotalInstanceCount(),
                         metrics.m1_cpu_submit_ms, metrics.m2_gpu_frame_ms,
                         metrics.m3_draw_calls, metrics.m4_cull_ms);
        }
        TRACY_SCOPE_END();
    };

    std::atomic<bool> running{true};
    lcf::PeriodicTask engine_task{
        1ms,
        std::move(render_loop),
        [&] { return running.load(std::memory_order_relaxed); },
    };
    auto & engine_scheduler = registry.ctx().get<lcf::TaskScheduler>();
    engine_scheduler.registerPeriodicTask(std::move(engine_task)).run();

    lcf::TaskScheduler window_scheduler{{lcf::TaskSchedulerRunMode::eThisThread}};
    lcf::PeriodicTask poll_task(
        5ms,
        [&] { window_up->pollEvents(); },
        [&] { return window_up->getState() != lcf::gui::WindowState::eAboutToClose; },
        [&] {
            window_scheduler.stop();
            running.store(false, std::memory_order_relaxed);
        }
    );
    window_scheduler.registerPeriodicTask(std::move(poll_task)).run();

    // 窗口已关闭、running=false。此时 engine_scheduler 上的渲染 PeriodicTask
    // 仍可能在做最后一帧 record/submit。先停 engine_scheduler，再 waitIdle，
    // 最后让 switcher / scene 在受控的析构顺序里释放 GPU 资源。
    //
    // 注意：engine_scheduler.stop() 不一定立即 join；为确保渲染线程不再触碰
    // switcher/scene，必须在 stop 之后再 waitIdle，且 waitIdle 之前 GPU 不再
    // 接收新工作。简单可靠的做法是 sleep 50ms 让 PeriodicTask 跳出当前迭代。
    engine_scheduler.stop();
    std::this_thread::sleep_for(50ms);
    context.getDevice().waitIdle();
    lcf_log_info("[bench] interactive loop exited cleanly, GPU idle, releasing resources...");

    return 0;
}
