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

    // 未来扩展变体支持时只需把 product_count 从 1 → N，variant_key 实际写入字节即可，无 breaking change。
    struct ProductRef
    {
        std::vector<std::byte> m_variant_key;
        uint64_t m_compile_input_hash = 0;
    };

    struct ManifestEntry
    {
        ManifestEntry() noexcept = default;
        ManifestEntry(std::filesystem::path canonical_source, slang::CompileSettings settings) noexcept
            : m_source_path(std::move(canonical_source))
            , m_main_fingerprint(m_source_path)
            , m_compile_settings(settings) {}

        std::filesystem::path m_source_path;
        ShaderFingerprint m_main_fingerprint;
        std::vector<std::pair<std::filesystem::path, ShaderFingerprint>> m_deps;
        slang::CompileSettings m_compile_settings; 
        std::vector<ProductRef> m_products;
    };

    struct ManifestPathHash
    {
        size_t operator()(const std::filesystem::path & p) const noexcept { return std::filesystem::hash_value(p); }
    };
    using ManifestEntryMap = tsl::robin_map<std::filesystem::path, ManifestEntry, ManifestPathHash>;

    class Manifest
    {
    public:
        static Manifest & instance() noexcept;
        ~Manifest() noexcept;
        Manifest(const Manifest &) = delete;
        Manifest & operator=(const Manifest &) = delete;
        Manifest(Manifest &&) = delete;
        Manifest & operator=(Manifest &&) = delete;
    public:
        const ManifestEntry * find(const std::filesystem::path & resolved_source) noexcept;
        void upsert(ManifestEntry entry) noexcept;
        std::error_code flush() noexcept;
    private:
        // 构造时立即从磁盘加载 manifest——单例只构造一次，等价于"启动时一次性加载"。
        // 不存在 / 损坏 / 版本不匹配 → 视作空 manifest。
        Manifest() noexcept;

    private:
        std::string m_loaded_slang_global_version;
        ManifestEntryMap m_entries;
        bool m_dirty = false;
    };

    std::filesystem::path normalize_manifest_path(const std::filesystem::path & path) noexcept;
}
