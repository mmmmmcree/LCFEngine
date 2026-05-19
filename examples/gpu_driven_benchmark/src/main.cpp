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
#include "RendererSwitcher.h"
#include "benchmark_types.h"

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
#include <vector>

using namespace std::chrono_literals;

namespace {
    // ------------- CLI -------------
    struct CliArgs
    {
        bool show_help    = false;
        bool show_version = false;
        bool self_test    = false;
        bool benchmark    = false;
        // 跑分参数（可选 override）
        uint32_t warmup_frames  = 200;
        uint32_t sample_frames  = 1000;
        // 默认输出目录：<exe>/benchmark_results/result_YYYYMMDD_HHMMSS.csv
        std::filesystem::path out_csv;
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
            else if (a == "--warmup" && i + 1 < argc) {
                args.warmup_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
            else if (a == "--samples" && i + 1 < argc) {
                args.sample_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
            else if (a == "--out" && i + 1 < argc) {
                args.out_csv = argv[++i];
            }
        }
        return args;
    }

    void print_help()
    {
        std::puts("gpu_driven_benchmark (LCFEngine experiment)");
        std::puts("Usage:");
        std::puts("  gpu_driven_benchmark                       interactive mode (default)");
        std::puts("    keys: 1/2/3  -> switch path");
        std::puts("    keys: F1-F4  -> switch scene scale (A/B/C/D)");
        std::puts("  gpu_driven_benchmark --benchmark           headless full-matrix bench");
        std::puts("    --warmup N         (default 200)");
        std::puts("    --samples N        (default 1000)");
        std::puts("    --out <path.csv>   (default benchmark_results/result_TS.csv)");
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
}

// ---------------- main ----------------
int main(int argc, char * argv[])
{
    lcf::Logger::init();
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
        // 相机用一组固定姿态使每个 (path, scene) 跑分时画面一致。

        std::filesystem::path csv_path = args.out_csv;
        if (csv_path.empty()) {
            csv_path = std::filesystem::path("benchmark_results") /
                       ("result_" + make_timestamp_suffix() + ".csv");
        }
        lcf_log_info("benchmark mode: warmup={} samples={} out={}",
                     args.warmup_frames, args.sample_frames, csv_path.string());

        lcf::benchmark::FrameMetricsCollector collector;

        // 跑分时主线程负责 pollEvents（让 swapchain 不被系统判死），
        // 渲染循环在主线程同步迭代（不开 PeriodicTask 异步引擎线程）。
        const lcf::benchmark::ePath  paths[]  = {
            lcf::benchmark::ePath::eCpuDrivenNaive,
            lcf::benchmark::ePath::eCpuDrivenIndirect,
            lcf::benchmark::ePath::eGpuDriven,
        };
        const lcf::benchmark::eScene scenes[] = {
            lcf::benchmark::eScene::eA,
            lcf::benchmark::eScene::eB,
            lcf::benchmark::eScene::eC,
            lcf::benchmark::eScene::eD,
        };

        // 给相机摆个固定姿态：稍后退便于看到所有实例。
        camera_transform.translateWorld(0.0f, 0.0f, 50.0f);
        registry.enqueueSignal<lcf::ecs::TransformUpdateSignal>({camera_entity.getId()});
        registry.ctx().get<lcf::ecs::Dispatcher>().update();
        transform_system.update();

        for (auto path : paths) {
            for (auto sc : scenes) {
                switcher.switchPath(path);
                scene.setSceneScale(sc);
                const uint32_t inst = scene.getTotalInstanceCount();

                // warmup
                for (uint32_t i = 0; i < args.warmup_frames; ++i) {
                    window_up->pollEvents();
                    if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto done; }
                    switcher.render(camera_entity, window_up->getEntity());
                }
                // sample
                collector.beginRun(path, sc, inst, args.sample_frames);
                for (uint32_t i = 0; i < args.sample_frames; ++i) {
                    window_up->pollEvents();
                    if (window_up->getState() == lcf::gui::WindowState::eAboutToClose) { goto done; }
                    auto m = switcher.render(camera_entity, window_up->getEntity());
                    collector.push(m);
                }
                collector.endRunAndAppendCsv(csv_path);
            }
        }
done:
        context.getDevice().waitIdle();
        lcf_log_info("benchmark done. csv: {}", csv_path.string());
        return 0;
    }

    // ---- 交互模式：双调度器，按键切路径/场景 ----
    // 注意：F1-F12 当前没在 SDLWindow.cpp 的 key map 里实现，所以场景切换
    // 改用数字键 4/5/6/7（A/B/C/D）。路径切换仍是 1/2/3。
    auto current_scene_idx = static_cast<uint32_t>(0);  // A
    std::array<lcf::benchmark::ePath, 3> path_keys = {
        lcf::benchmark::ePath::eCpuDrivenNaive,
        lcf::benchmark::ePath::eCpuDrivenIndirect,
        lcf::benchmark::ePath::eGpuDriven,
    };
    bool prev_key_state[7] = {false, false, false, false, false, false, false};  // 1/2/3/4/5/6/7
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
        }
        if (check_press_edge(lcf::KeyboardKey::e2, 1)) {
            lcf_log_info("[bench] switching to path={} (may pause briefly for shader compile)",
                         lcf::benchmark::to_csv_name(path_keys[1]));
            switcher.switchPath(path_keys[1]);
        }
        if (check_press_edge(lcf::KeyboardKey::e3, 2)) {
            lcf_log_info("[bench] switching to path={}", lcf::benchmark::to_csv_name(path_keys[2]));
            switcher.switchPath(path_keys[2]);
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

        // 简易 HUD：每 30 帧打一次指标到日志（不阻塞主循环）。
        static uint32_t hud_counter = 0;
        if ((++hud_counter % 30) == 0) {
            lcf_log_info("path={} scene_inst={} M1={:.3f}ms M2={:.3f}ms M3={} M4_cull={:.3f}ms",
                         lcf::benchmark::to_csv_name(switcher.getActivePath()),
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
