#pragma once

#include "shader_core_enums.h"
#include "config.h"
#include "concepts/range_concept.h"
#include <tsl/robin_map.h>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include <ranges>
#include <system_error>
#include <unordered_set>

namespace lcf::sc {

    struct FileFingerprint
    {
        FileFingerprint() noexcept = default;
        explicit FileFingerprint(const std::filesystem::path & path) noexcept;
        bool operator==(const FileFingerprint & other) const noexcept = default;
        bool matches(const std::filesystem::path & path) const noexcept;

        mutable uint64_t m_mtime = 0;
        uint64_t m_file_size = 0;
        uint64_t m_content_hash = 0;
    };

    class FileRecord
    {
    public:
        FileRecord() noexcept = default;
        explicit FileRecord(std::filesystem::path path) noexcept;
        FileRecord(std::filesystem::path path, const FileFingerprint & fingerprint) noexcept;
        const std::filesystem::path & getPath() const noexcept { return m_path; }
        const uint64_t & getPathHash() const noexcept { return m_path_hash; }
        const FileFingerprint & getFingerprint() const noexcept { return m_fingerprint; }
        bool isOutdated() const noexcept { return not m_fingerprint.matches(m_path); }
        bool isSamePath(const FileRecord & other) const noexcept { return m_path_hash == other.m_path_hash and m_path == other.m_path; }
        void refresh() noexcept { m_fingerprint = FileFingerprint(m_path); }
    private:
        std::filesystem::path m_path;
        uint64_t m_path_hash = 0;
        FileFingerprint m_fingerprint;
    };

    // 未来扩展变体支持时只需把 product_count 从 1 → N，variant_key 实际写入字节即可，无 breaking change。
    struct ProductRef
    {
        ProductRef() noexcept = default;
        ProductRef(uint64_t compile_input_hash) noexcept : m_compile_input_hash(compile_input_hash) {}

        const uint64_t & getCompileInputHash() const noexcept { return m_compile_input_hash; }

        std::vector<std::byte> m_variant_key;
        uint64_t m_compile_input_hash = 0;
    };

    class ManifestEntry
    {
        using Self = ManifestEntry;
        using DependencyRecordList = std::vector<FileRecord>;
        using ProductList = std::vector<ProductRef>;
    public:
        ManifestEntry() noexcept = default;
        explicit ManifestEntry(const std::filesystem::path & source_path) noexcept;
        Self & addDependency(const std::filesystem::path & dependency_path);
        Self & appendDependencies(convertible_range_of_c<std::filesystem::path> auto && dependencies)
        {
            if constexpr (std::ranges::sized_range<decltype(dependencies)>) {
                m_dependency_records.reserve(m_dependency_records.size() + std::ranges::size(dependencies));
            }
            for (auto && dependency : dependencies) {
                this->addDependency(std::filesystem::path(std::forward<decltype(dependency)>(dependency)));
            }
            return *this;
        }
        Self & addDependencyRecord(FileRecord record);
        Self & addProduct(const ProductRef & product) noexcept;
        Self & setMainRecord(FileRecord record) noexcept;
        Self & setCompileSettings(const sl::CompileSettings & settings) noexcept;
        const FileRecord & getMainRecord() const noexcept { return m_main_record; }
        const DependencyRecordList & getDependencyRecords() const noexcept { return m_dependency_records; };
        const sl::CompileSettings & getCompileSettings() const noexcept { return m_compile_settings; }
        const ProductList & getProducts() const noexcept { return m_products; }
        bool isOutdated() const noexcept;
    private:
        FileRecord m_main_record;
        DependencyRecordList m_dependency_records;
        sl::CompileSettings m_compile_settings;
        ProductList m_products;
    };

    using ManifestEntryMap = tsl::robin_map<std::filesystem::path, ManifestEntry>;

    class Manifest
    {
    public:
        ~Manifest() noexcept;
        explicit Manifest(std::filesystem::path work_dir) noexcept;
        Manifest(const Manifest &) = delete;
        Manifest & operator=(const Manifest &) = delete;
        Manifest(Manifest &&) = delete;
        Manifest & operator=(Manifest &&) = delete;
    public:
        const ManifestEntry * find(const std::filesystem::path & source_path) noexcept;
        void upsert(ManifestEntry entry) noexcept;
        std::error_code flush() noexcept;
        std::error_code shutdown() noexcept;
    private:
        std::filesystem::path m_work_dir;
        std::string m_loaded_slang_global_version;
        ManifestEntryMap m_entries;
        std::unordered_set<uint64_t> m_pending_orphan_hashes;
        bool m_dirty = false;
    };

    class ManifestManager
    {
    public:
        static ManifestManager & instance() noexcept;
        void registerStaticManifest(Manifest * manifest) noexcept;
        void shutdown() noexcept;
    private:
        std::vector<Manifest *> m_registered_manifests;
    };
}
