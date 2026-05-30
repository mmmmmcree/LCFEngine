#include "shader_core/config.h"
#include "shader_core/Manifest.h"
#include "shader_core/ShaderCompiler.h"
#include "log.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <optional>

using namespace std::chrono;
using namespace lcf;
using namespace lcf::sc;

namespace {

    namespace fs = std::filesystem;

    // 一次编译的耗时（毫秒）+ 是否成功。
    struct CompileResult
    {
        long long elapsed_ms = 0;
        bool ok = false;
    };

    CompileResult timed_compile(ShaderCompiler & compiler, const std::string & virtual_path)
    {
        auto t0 = system_clock::now();
        auto compiled = compiler.compileSlangSourceToSpv(virtual_path);
        auto elapsed = duration_cast<milliseconds>(system_clock::now() - t0).count();
        if (not compiled) {
            lcf_log_error("compile failed for '{}': {}", virtual_path, compiled.error().message());
            return { elapsed, false };
        }
        return { elapsed, true };
    }

    // RAII 备份恢复 .slang 文件，确保异常路径下也不会污染 git 工作树。
    // 也提供显式 restore()：测试主流程会在 Phase 8 主动调用，让 manifest 最终记录的是干净版本的指纹，
    // 这样跨进程再跑一次时 P1 才能 HIT；析构是兜底，仅在异常退出时生效。
    class FileBackupGuard
    {
    public:
        explicit FileBackupGuard(fs::path target) : m_target(std::move(target))
        {
            std::error_code ec;
            if (not fs::exists(m_target, ec) or ec) { return; }
            auto bytes_or = read_file_as_bytes_local(m_target);
            if (bytes_or.has_value()) {
                m_backup = std::move(bytes_or.value());
                m_active = true;
            }
        }
        ~FileBackupGuard() noexcept { restore(); }

        // 显式恢复（多次调用幂等：第一次写回原内容并失活；之后调用无操作）。
        void restore() noexcept
        {
            if (not m_active) { return; }
            std::ofstream out(m_target, std::ios::binary | std::ios::trunc);
            if (out.is_open()) {
                out.write(reinterpret_cast<const char *>(m_backup.data()), static_cast<std::streamsize>(m_backup.size()));
            }
            m_active = false;
        }

        FileBackupGuard(const FileBackupGuard &) = delete;
        FileBackupGuard & operator=(const FileBackupGuard &) = delete;

    private:
        // 内嵌一个本地的简易读取（避免在 example 里依赖 utilities/file_utils 的 internal 类型选择）。
        static std::optional<std::vector<char>> read_file_as_bytes_local(const fs::path & p)
        {
            std::ifstream in(p, std::ios::binary | std::ios::ate);
            if (not in.is_open()) { return std::nullopt; }
            auto size = static_cast<std::streamsize>(in.tellg());
            std::vector<char> buf(size);
            in.seekg(0);
            in.read(buf.data(), size);
            if (not in.good()) { return std::nullopt; }
            return buf;
        }

    private:
        fs::path m_target;
        std::vector<char> m_backup;
        bool m_active = false;
    };

    // 向文件追加一行固定标记。**不能含时间戳/运行时变量**，否则每次跑算出的 cache_hash 都不同，
    // 会让 .shader_cache/ 累积一堆永远复用不到的 spvbin（manifest 没有 GC 之前等于内存泄漏）。
    void append_marker(const fs::path & p)
    {
        std::ofstream out(p, std::ios::binary | std::ios::app);
        if (not out.is_open()) {
            lcf_log_error("failed to open '{}' for append", p.string());
            return;
        }
        out << "\n// touched by shader_core_test\n";
    }

    void log_phase(int phase, std::string_view title)
    {
        lcf_log_info("=== Phase {}: {} ===", phase, title);
    }

    void compile_abc(ShaderCompiler & compiler, std::string_view phase_tag)
    {
        for (const auto & name : { "shader_a", "shader_b", "shader_c" }) {
            auto path = std::string("test_shaders://") + name + ".slang";
            auto r = timed_compile(compiler, path);
            lcf_log_info("[{}] {} -> {} ms ({})", phase_tag, name, r.elapsed_ms, r.ok ? "OK" : "FAIL");
        }
    }
}

int main()
{
    lcf::log::init();

    fs::path assets_dir = LCF_EXAMPLES_ASSETS_DIR;
    fs::path test_shaders_dir = LCF_SHADER_CORE_TEST_SHADERS_DIR;

    sc::Config::instance()
        .registerVirtualPath("shaders", assets_dir / "shaders")
        .registerVirtualPath("test_shaders", test_shaders_dir)
        // include 目录里加 test_shaders_dir，slang 才能解析 `import common`。
        .addIncludeDirectory(test_shaders_dir);

    fs::path common_path     = test_shaders_dir / "common.slang";
    fs::path shader_c_path   = test_shaders_dir / "shader_c.slang";

    // 注：为了让 RAII 备份在异常退出时也能恢复，把所有 guard 放在 main 顶部作用域。
    // 它们在 main 退出时才析构，所以阶段 4/6 的 append 会在阶段 5/7 之前生效，最后才被恢复。
    FileBackupGuard guard_c(shader_c_path);
    FileBackupGuard guard_common(common_path);

    ShaderCompiler compiler;

    // ---------- Phase 1: 第一轮编译 a/b/c ----------
    // 跨进程命中验证：第一次跑通常 MISS（用户清掉 .shader_cache 后），
    // 第二次以后跑应当全 HIT（manifest + spvbin 都已落盘）。
    log_phase(1, "compile a/b/c (first round; MISS if cache empty, HIT if cross-process cache exists)");
    compile_abc(compiler, "P1");

    // ---------- Phase 2: 同一进程内连编 a/b/c 第二次 ----------
    // 验证进程内 manifest 命中稳定性。manifest 在 Phase1 MISS 后已 upsert 到内存，
    // 这里 find 会命中，只走 ShaderCache::tryLoad（spvbin 磁盘读）。
    log_phase(2, "compile a/b/c again (in-process repeated, expect 3x HIT)");
    compile_abc(compiler, "P2");

    // ---------- Phase 3: sphere_to_cube 单文件回归 ----------
    log_phase(3, "compile sphere_to_cube (single-file regression, no deps)");
    {
        auto r = timed_compile(compiler, "shaders://sphere_to_cube.slang");
        lcf_log_info("[P3] sphere_to_cube -> {} ms ({})", r.elapsed_ms, r.ok ? "OK" : "FAIL");
    }

    // 阶段 1-3 完成，强制 flush manifest：模拟跨进程边界（即使本进程崩溃，下次启动也能继续验证 HIT）。
    if (auto ec = sc::Manifest::instance().flush()) {
        lcf_log_warn("flushShaderManifest after phase 3 failed: {}", ec.message());
    } else {
        lcf_log_info("manifest flushed to disk after phase 3");
    }

    // ---------- Phase 4 + 5: 改 shader_c 后再编 a/b/c ----------
    log_phase(4, "append marker to shader_c.slang (should affect only shader_c)");
    append_marker(shader_c_path);
    log_phase(5, "compile a/b/c (expect a HIT, b HIT, c MISS)");
    compile_abc(compiler, "P5");

    // ---------- Phase 6 + 7: 改 common 后再编 a/b/c ----------
    log_phase(6, "append marker to common.slang (should cascade-invalidate a/b/c)");
    append_marker(common_path);
    log_phase(7, "compile a/b/c (expect 3x MISS due to cascade)");
    compile_abc(compiler, "P7");

    // ---------- Phase 8: 显式恢复 + 再编一遍，让 manifest 最终记录干净版本指纹 ----------
    log_phase(8, "restore shader_c/common to clean state, recompile a/b/c so manifest records clean fingerprints (next run can HIT)");
    guard_c.restore();
    guard_common.restore();
    compile_abc(compiler, "P8");

    // 显式 shutdown：先按 setGcOnShutdown 决定是否 GC，再 flush 落盘。
    // 不能依赖 ~Manifest()——在 DLL/Meyers-singleton 场景下静态局部对象的析构
    // 在某些 toolchain 上不会被触发，会导致 GC 与最终 flush 都被跳过。
    if (auto ec = sc::Manifest::instance().shutdown()) {
        lcf_log_warn("Manifest shutdown failed: {}", ec.message());
    }
    return 0;
}
