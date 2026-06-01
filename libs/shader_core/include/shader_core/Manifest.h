#pragma once

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
#include <unordered_map>
#include <optional>

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

    class ManifestEntry
    {
        using Self = ManifestEntry;
        using FileRecordList = std::vector<FileRecord>;
        using ProductHashMap = tsl::robin_map<std::string, uint64_t>;
    public:
        ManifestEntry() noexcept = default;
        template <convertible_range_of_c<FileRecord> FileRange>
        explicit ManifestEntry(FileRange && file_range) noexcept :
            m_file_records(std::forward<FileRange>(file_range) | std::ranges::to<FileRecordList>()) {}
        template <convertible_range_of_c<std::filesystem::path> PathRange>
        explicit ManifestEntry(PathRange && path_range) noexcept :
            m_file_records(std::forward<PathRange>(path_range) | std::ranges::to<FileRecordList>()) {}
        const FileRecordList & getFileRecords() const noexcept { return m_file_records; }
        auto getProductHashes() const noexcept { return m_product_hashes | std::views::values; }
        ProductHashMap & getProductHashMap() noexcept { return m_product_hashes; }
        const ProductHashMap & getProductHashMap() const noexcept { return m_product_hashes; }
        std::optional<uint64_t> getProductHash(const std::string & product_key) const noexcept;
        bool isOutdated() const noexcept;
        void addProductHash(const std::string & product_key, uint64_t product_hash) { m_product_hashes.emplace(std::make_pair(product_key, product_hash)); }
    private:
        FileRecordList m_file_records;
        ProductHashMap m_product_hashes;
    };

    using ManifestEntryMap = tsl::robin_map<std::filesystem::path, ManifestEntry>;

    class Manifest
    {
    public:
        ~Manifest() noexcept;
        Manifest(std::filesystem::path work_dir, std::filesystem::path cache_ext) noexcept;
        Manifest(const Manifest &) = delete;
        Manifest & operator=(const Manifest &) = delete;
        Manifest(Manifest &&) = delete;
        Manifest & operator=(Manifest &&) = delete;
    public:
        const ManifestEntry * find(const std::filesystem::path & source_path) noexcept;
        void upsert(const std::filesystem::path & source_path, ManifestEntry entry) noexcept;
        std::error_code shutdown() noexcept;
    private:
        std::filesystem::path m_work_dir;
        std::filesystem::path m_cache_ext;
        ManifestEntryMap m_entries;
        std::unordered_set<uint64_t> m_pending_orphan_hashes;
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
