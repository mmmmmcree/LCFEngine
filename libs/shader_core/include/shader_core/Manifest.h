#pragma once

#include "shader_core_enums.h"
#include "config.h"
#include <tsl/robin_map.h>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include <system_error>

namespace lcf::shader_core {

    // 文件指纹：mtime + file_size 作快路径，content_hash 作兜底（mtime/size 不一致时再算）。
    // 默认构造（全 0）代表"未记录/不存在"，与已存在文件的指纹不可能相等——可作 MISS 哨兵。
    struct ShaderFingerprint
    {
        ShaderFingerprint() noexcept = default;
        explicit ShaderFingerprint(const std::filesystem::path & path) noexcept;
        bool operator==(const ShaderFingerprint & other) const noexcept = default;
        bool matches(const std::filesystem::path & path) const noexcept;

        uint64_t m_mtime = 0;
        uint64_t m_file_size = 0;
        uint64_t m_content_hash = 0;
    };

    // 单个产物引用。第一版每个 entry 仅含 1 个 product，variant_key 长度恒为 0。
    // 未来扩展变体支持时只需把 product_count 从 1 → N，variant_key 实际写入字节即可，无 breaking change。
    struct ProductRef
    {
        std::vector<std::byte> m_variant_key;   // 第一版恒为空
        uint64_t m_compile_input_hash = 0;      // 用于查 ShaderCache::tryLoad 的 key
    };

    // 一条 manifest entry：路径 → 指纹链 → 产物引用。
    // 命中判定流程：
    //   1) m_compile_settings 与当前 slang::Config 一致
    //   2) m_main_fingerprint.matches(m_source_path)
    //   3) 所有 m_deps[i].second.matches(m_deps[i].first)
    //   4) ShaderCache::tryLoad(m_products[0].m_compile_input_hash) 命中
    // 任一步失败 → MISS，走完整 slang 编译路径。
    struct ManifestEntry
    {
        ManifestEntry() noexcept = default;

        // 用归一化后的源文件路径 + 编译设置直接填 source_path / main_fingerprint / compile_settings 三个字段。
        // 调用方只需再补上 deps 与 products 即可——避免每次 MISS 路径手抠三行赋值。
        ManifestEntry(std::filesystem::path canonical_source, slang::CompileSettings settings) noexcept
            : m_source_path(std::move(canonical_source))
            , m_main_fingerprint(m_source_path)
            , m_compile_settings(settings) {}

        std::filesystem::path m_source_path;   // resolved + weakly_canonical 后的绝对路径
        ShaderFingerprint m_main_fingerprint;
        std::vector<std::pair<std::filesystem::path, ShaderFingerprint>> m_deps;
        slang::CompileSettings m_compile_settings;   // profile + flags（与 slang version 区分；version 在 manifest 全局头）
        std::vector<ProductRef> m_products;    // 第一版 size==1
    };

    // ninja 风格的 shader 增量编译判定层。
    //
    // 设计要点：
    // - 单例（与 Config::instance() 对称），全进程共享磁盘状态映射
    // - 启动时 lazy load manifest.bin（首次 find/upsert 触发），不存在/损坏/版本不匹配则视作空 manifest
    // - 内存 dirty 标记 + batch flush：MISS 编译完成后只改内存，析构时一次性 flush
    // - 显式 flush() 接口供测试模拟跨进程边界 / 未来热重载强制落盘
    //
    // 二进制格式（version 1）：
    //   header { magic:u32 version:u32 slang_global_version:lstring entry_count:u32 }
    //   entry  { entry_size:u32  // 整条 entry 字节数（不含 entry_size 自身），用于前向兼容跳过未知字段
    //            source_path:lstring
    //            main_fingerprint:{ mtime:u64 size:u64 content_hash:u64 }
    //            dep_count:u32
    //            deps[dep_count]:{ path:lstring mtime:u64 size:u64 content_hash:u64 }
    //            slang_target_profile:u8
    //            slang_compiler_options:u32
    //            product_count:u32
    //            products[product_count]:{ variant_key_size:u32 variant_key_bytes:[u8] compile_input_hash:u64 }
    //          }
    //   lstring = { len:u32, bytes:[u8] }
    class Manifest
    {
    public:
        static Manifest & instance() noexcept;

        Manifest(const Manifest &) = delete;
        Manifest & operator=(const Manifest &) = delete;
        Manifest(Manifest &&) = delete;
        Manifest & operator=(Manifest &&) = delete;
        ~Manifest() noexcept;

    public:
        // 按归一化后的绝对路径查找 entry。返回 nullptr 表示未记录（并非 MISS 判定本身——指纹校验在调用方）。
        // 内部按需 lazy load。
        const ManifestEntry * find(const std::filesystem::path & resolved_source) noexcept;

        // 写入或覆盖 entry。只改内存 + 标 dirty，不立即写盘。
        void upsert(ManifestEntry entry) noexcept;

        // 强制把内存 manifest 落盘到 Config::getCacheDirectory()/manifest.bin。
        // 析构时也会自动调用一次。
        std::error_code flush() noexcept;

    private:
        Manifest() noexcept = default;

        void ensureLoaded() noexcept;      // lazy load 入口
        void loadFromDisk() noexcept;      // 实际反序列化
        std::error_code writeToDisk() const; // 实际序列化

    private:
        struct PathHash { size_t operator()(const std::filesystem::path & p) const noexcept { return std::filesystem::hash_value(p); } };
        tsl::robin_map<std::filesystem::path, ManifestEntry, PathHash> m_entries;
        bool m_loaded = false;
        bool m_dirty = false;
        std::string m_loaded_slang_global_version;  // 加载时记录，flush 时若与当前 slang version 不同则按当前写
    };

    // 把任意路径归一化为 manifest key 的工具：先 Config::resolvePath（解析虚拟路径）再 weakly_canonical。
    // 失败时退化为 absolute（避免文件不存在导致 weakly_canonical 抛异常）。
    std::filesystem::path normalize_manifest_path(const std::filesystem::path & path) noexcept;
}
