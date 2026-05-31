#include "shader_core/Manifest.h"
#include "shader_core/buffer_io.h"
#include "shader_core/hash.h"
#include "file_utils.h"
#include "bytes.h"
#include <chrono>
#include <ranges>
#include <algorithm>
#include <unordered_set>
#include <format>

using namespace lcf;
using namespace lcf::sc;
namespace stdfs = std::filesystem;
namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {
    constexpr uint32_t k_version = 3;
    constexpr std::string_view k_manifest_file_ext {".mnfbin"};

    struct ManifestHeader
    {
        ManifestHeader() noexcept = default;
        ManifestHeader(uint32_t version, uint32_t entry_count) noexcept :
            m_version(version),
            m_entry_count(entry_count) {}
        
        const uint32_t & getVersion() const noexcept { return m_version; }
        const uint32_t & getEntryCount() const noexcept { return m_entry_count; }

        uint32_t m_version = 0;
        uint32_t m_entry_count = 0;
    };

    struct ManifestEntryHeader
    {
        ManifestEntryHeader() noexcept = default;
        ManifestEntryHeader(uint32_t file_record_count, uint32_t product_hash_count) noexcept : 
            m_file_record_count(file_record_count),
            m_product_hash_count(product_hash_count) {}
        
        const uint32_t & getFileRecordCount() const noexcept { return m_file_record_count; }
        const uint32_t & getProductHashCount() const noexcept { return m_product_hash_count; }

        uint32_t m_file_record_count = 0;
        uint32_t m_product_hash_count = 0;
    };

    struct FileRecordHeader
    {
        FileRecordHeader() noexcept = default;
        FileRecordHeader(uint32_t path_size_in_bytes) noexcept :
            m_path_size_in_bytes(path_size_in_bytes) {}
        
        const uint32_t & getPathSizeInBytes() const noexcept { return m_path_size_in_bytes; }

        uint32_t m_path_size_in_bytes = 0;
    };

    struct ProductEntryHeader
    {
        ProductEntryHeader() noexcept = default;
        ProductEntryHeader(uint32_t key_size_in_bytes, uint64_t product_hash) noexcept :
            m_key_size_in_bytes(key_size_in_bytes), m_product_hash(product_hash) {}

        uint32_t m_key_size_in_bytes = 0;
        uint32_t m_reserved = 0;
        uint64_t m_product_hash = 0;
    };
}

static stdfs::path make_manifest_file_path(const stdfs::path & work_dir) noexcept
{
    return work_dir / k_manifest_file_ext;
}

static stdfs::path cache_path_for_hash(const stdfs::path & work_dir, uint64_t hash, const stdfs::path & cache_ext) noexcept
{
    return work_dir / std::format("{:016x}{}", hash, cache_ext.string());
}

static uint64_t hash_file_content(const stdfs::path & path) noexcept
{
    auto expected_bytes = read_file_as_bytes(path);
    if (not expected_bytes) { return 0; }
    return sc::hash(as_const_bytes(expected_bytes.value()));
}

static stdfs::path normalize_manifest_path(const stdfs::path & resolved_path) noexcept
{
    std::error_code ec;
    auto canonical_path = stdfs::weakly_canonical(resolved_path, ec);
    if (ec) {
        auto abs_path = stdfs::absolute(resolved_path, ec);
        return ec ? resolved_path : abs_path;
    }
    return canonical_path;
}

static bool read_manifest_entry(BufferReader & reader, const ManifestEntryHeader & entry_header, ManifestEntry & out_entry) noexcept;

static void remove_cache_garbage(const stdfs::path & cache_dir, const std::unordered_set<uint64_t> & live_hashes) noexcept;

static ManifestEntryMap read_manifest_from_disk(const stdfs::path & work_dir, std::unordered_set<uint64_t> & orphan_hashes) noexcept;

static std::error_code write_manifest_to_disk(const stdfs::path & work_dir, const ManifestEntryMap & entries) noexcept;


FileFingerprint::FileFingerprint(const stdfs::path & path) noexcept
{
    std::error_code ec;
    if (not stdfs::exists(path, ec) or ec) { return; }
    auto file_size = stdfs::file_size(path, ec);
    if (ec) { return; }
    m_file_size = static_cast<uint64_t>(file_size);
    auto mt = stdfs::last_write_time(path, ec);
    if (not ec) {
        m_mtime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(mt.time_since_epoch()).count());
    }
    m_content_hash = hash_file_content(path);
}

bool FileFingerprint::matches(const stdfs::path & path) const noexcept
{
    std::error_code ec;
    if (not stdfs::exists(path, ec) or ec) { return false; }
    auto file_size = stdfs::file_size(path, ec);
    if (ec or m_file_size != static_cast<uint64_t>(file_size)) { return false; }
    auto mt = stdfs::last_write_time(path, ec);
    if (ec) { return false; }
    uint64_t mtime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(mt.time_since_epoch()).count());
    if (not ec and m_mtime == mtime) { return true; }
    if (hash_file_content(path) != m_content_hash) { return false; }
    m_mtime = mtime;
    return true;
}

Manifest::Manifest(stdfs::path work_dir, stdfs::path cache_ext) noexcept :
    m_work_dir(std::move(work_dir)),
    m_cache_ext(std::move(cache_ext)),
    m_entries(read_manifest_from_disk(m_work_dir, m_pending_orphan_hashes))
{
}

Manifest::~Manifest() noexcept
{
    this->shutdown();
}

const ManifestEntry * Manifest::find(const stdfs::path & source_path) noexcept
{
    auto canonical_path = normalize_manifest_path(source_path);
    auto it = m_entries.find(canonical_path);
    if (it == m_entries.end()) { return nullptr; }
    return &it->second;
}

static bool same_dependency_paths(const ManifestEntry & lhs, const ManifestEntry & rhs) noexcept
{
    const auto & lhs_records = lhs.getFileRecords();
    const auto & rhs_records = rhs.getFileRecords();
    if (lhs_records.size() != rhs_records.size()) { return false; }
    for (uint32_t i = 0; i < lhs_records.size(); ++i) {
        const auto & lhs_record = lhs_records[i];
        const auto & rhs_record = rhs_records[i];
        if (lhs_record.getPathHash() != rhs_record.getPathHash()) { return false; }
        if (lhs_record.getPath() != rhs_record.getPath()) { return false; }
    }
    return true;
}

void Manifest::upsert(const stdfs::path & source_path, ManifestEntry entry) noexcept
{
    auto canonical_path = normalize_manifest_path(source_path);
    auto it = m_entries.find(canonical_path);
    if (it == m_entries.end()) {
        m_entries.emplace(canonical_path, std::move(entry));
        return;
    }
    m_pending_orphan_hashes.insert_range(it->second.getProductHashes());
    if (same_dependency_paths(it->second, entry)) {
        auto & existing_map = it.value().getProductHashMap();
        for (auto & [key, hash] : entry.getProductHashMap()) {
            existing_map.insert_or_assign(std::move(key), hash);
        }
    } else {
        it.value() = std::move(entry);
    }
}

std::error_code Manifest::shutdown() noexcept
{
    std::unordered_set<uint64_t> live_hashes;
    for (const auto & [_, entry] : m_entries) {
        live_hashes.insert_range(entry.getProductHashes());
    }
    for (auto hash : m_pending_orphan_hashes) {
        if (live_hashes.contains(hash)) { continue; }
        stdfs::remove(cache_path_for_hash(m_work_dir, hash, m_cache_ext));
    }
    m_pending_orphan_hashes.clear();
    std::error_code ec;
    std::size_t file_count = std::distance(stdfs::directory_iterator(m_work_dir, ec), {});
    if (not ec and file_count > live_hashes.size() + 1u) { remove_cache_garbage(m_work_dir, live_hashes); }
    return write_manifest_to_disk(m_work_dir, m_entries);
}

FileRecord::FileRecord(stdfs::path path) noexcept :
    m_path(std::move(path)),
    m_path_hash(stdfs::hash_value(m_path)),
    m_fingerprint(m_path)
{
}

FileRecord::FileRecord(stdfs::path path, const FileFingerprint &fingerprint) noexcept :
    m_path(std::move(path)),
    m_path_hash(stdfs::hash_value(m_path)),
    m_fingerprint(fingerprint)
{
}

std::optional<uint64_t> ManifestEntry::getProductHash(const std::string &product_key) const noexcept
{
    auto it = m_product_hashes.find(product_key);
    if (it == m_product_hashes.end()) { return {}; }
    return it->second;
}

bool ManifestEntry::isOutdated() const noexcept
{
    return stdr::any_of(m_file_records, [](const auto & record) { return record.isOutdated(); });
}

ManifestManager & ManifestManager::instance() noexcept
{
    static ManifestManager s_instance;
    return s_instance;
}

void ManifestManager::registerStaticManifest(Manifest *manifest) noexcept
{
    m_registered_manifests.emplace_back(manifest);
}

void ManifestManager::shutdown() noexcept
{
    for (auto manifest : m_registered_manifests) {
        manifest->shutdown();
    }
}

static void remove_cache_garbage(const stdfs::path & cache_dir, const std::unordered_set<uint64_t> & live_hashes) noexcept
{
    std::error_code ec;
    if (not stdfs::is_directory(cache_dir, ec) or ec) { return; }
    auto iter = stdfs::directory_iterator(cache_dir, ec), end = stdfs::directory_iterator{};
    for (; not ec and iter != end; iter.increment(ec)) {
        const auto & dir_entry = *iter;
        if (not dir_entry.is_regular_file(ec) or ec) { ec.clear(); continue; }
        if (dir_entry.path().extension() == k_manifest_file_ext) { continue; }
        const auto stem = dir_entry.path().stem().string();
        uint64_t hash = 0;
        try { hash = std::stoull(stem, nullptr, 16); }
        catch (...) { continue; }
        if (live_hashes.contains(hash)) { continue; }
        stdfs::remove(dir_entry.path());
    }
}


static bool read_manifest_entry(BufferReader & reader, const ManifestEntryHeader & entry_header, ManifestEntry & entry) noexcept
{
    std::vector<FileRecord> file_records;
    file_records.reserve(entry_header.getFileRecordCount());
    for (uint32_t i = 0; i < entry_header.getFileRecordCount(); ++i) {
        FileRecordHeader file_header;
        if (not reader.read(file_header)) { return false; }
        std::string path_str(file_header.getPathSizeInBytes(), '\0');
        if (not reader.readBytes(as_bytes(path_str))) { return false; }
        FileFingerprint fingerprint;
        if (not reader.read(fingerprint)) { return false; }
        file_records.emplace_back(stdfs::path(path_str), fingerprint);
    }
    entry = ManifestEntry(std::move(file_records));
    for (uint32_t i = 0; i < entry_header.getProductHashCount(); ++i) {
        ProductEntryHeader prod_header;
        if (not reader.read(prod_header)) { return false; }
        std::string key(prod_header.m_key_size_in_bytes, '\0');
        if (not reader.readBytes(as_bytes(key))) { return false; }
        entry.addProductHash(key, prod_header.m_product_hash);
    }
    return true;
}

static ManifestEntryMap read_manifest_from_disk(const stdfs::path & work_dir, std::unordered_set<uint64_t> & orphan_hashes) noexcept
{
    auto path = make_manifest_file_path(work_dir);
    auto expected_bytes = read_file_as_bytes(path);
    if (not expected_bytes) { return {}; }
    BufferReader reader(expected_bytes.value());
    ManifestHeader header;
    if (not reader.read(header)) { return {}; }
    if (header.getVersion() != k_version) { return {}; }
    ManifestEntryMap entries;
    for (uint32_t i = 0; i < header.getEntryCount(); ++i) {
        uint32_t key_path_size = 0;
        if (not reader.read(key_path_size)) { break; }
        std::string key_path_str(key_path_size, '\0');
        if (not reader.readBytes(as_bytes(key_path_str))) { break; }
        ManifestEntryHeader entry_header;
        if (not reader.read(entry_header)) { break; }
        ManifestEntry entry;
        if (not read_manifest_entry(reader, entry_header, entry)) { break; }
        stdfs::path key_path(key_path_str);
        std::error_code ec;
        if (not entry.getFileRecords().empty() and not stdfs::exists(entry.getFileRecords().front().getPath(), ec)) {
            orphan_hashes.insert_range(entry.getProductHashes());
            continue;
        }
        entries.insert_or_assign(std::move(key_path), std::move(entry));
    }
    return entries;
}

static std::error_code write_manifest_to_disk(const stdfs::path & work_dir, const ManifestEntryMap & entries) noexcept
{
    std::error_code ec;
    stdfs::create_directories(work_dir, ec);
    if (ec) { return ec; }
    BufferWriter writer;
    writer.write(ManifestHeader{k_version, static_cast<uint32_t>(entries.size())});
    for (const auto & [key_path, entry] : entries) {
        auto key_path_str = key_path.string();
        uint32_t key_path_size = static_cast<uint32_t>(key_path_str.size());
        writer.write(key_path_size);
        writer.writeBytes(as_bytes(key_path_str));
        writer.write(ManifestEntryHeader{
            static_cast<uint32_t>(entry.getFileRecords().size()),
            static_cast<uint32_t>(entry.getProductHashMap().size())
        });
        for (const auto & file_record : entry.getFileRecords()) {
            auto path_str = file_record.getPath().string();
            writer.write(FileRecordHeader{static_cast<uint32_t>(path_str.size())});
            writer.writeBytes(as_bytes(path_str));
            writer.write(file_record.getFingerprint());
        }
        for (const auto & [key, hash] : entry.getProductHashMap()) {
            writer.write(ProductEntryHeader{static_cast<uint32_t>(key.size()), hash});
            writer.writeBytes(as_bytes(key));
        }
    }
    return write_file(make_manifest_file_path(work_dir), as_bytes(writer.getBuffer()));
}