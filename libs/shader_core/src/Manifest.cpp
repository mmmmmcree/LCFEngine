#include "shader_core/Manifest.h"
#include "shader_core/buffer_io.h"
#include "shader_core/config.h"
#include "shader_core/hash.h"
#include "file_utils.h"
#include "bytes.h"
#include <slang.h>
#include <chrono>
#include <ranges>
#include <algorithm>
#include <unordered_set>
#include <cctype>
#include <format>
#include <set>

using namespace lcf;
using namespace lcf::sc;
namespace stdfs = std::filesystem;
namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {
    constexpr uint32_t k_magic = 0x4C434D46; // "LCMF"
    constexpr uint32_t k_version = 2;        // v2: header-first 紧凑布局（每个变长域前都先甩一个定长 header struct）
    constexpr std::string_view k_manifest_file_name {".mnfbin"};

    /* ===== manifest file layout =====
        ManifestHeader  { magic; version; slang_ver_size; entry_count; }
        slang_ver_bytes [slang_ver_size]
        for each entry:
            EntryHeader { entry_size_in_bytes; source_path_size; dep_count; product_count;
                            profile_raw; options_raw; _reserved; }
            source_path_bytes [source_path_size]
            main_fingerprint  (ShaderFingerprint，trivially copyable)
            for each dep:
                DependencyHeader { dep_path_size; fingerprint; }
                dep_path_bytes [dep_path_size]
            for each product:
                ProductHeader { variant_key_size; compile_input_hash; }
                variant_key_bytes [variant_key_size]

        entry_size_in_bytes 表示"EntryHeader 之后到下一个 EntryHeader 之前"的所有字节数，用于前向兼容：
        未来若新增字段，旧 reader 读完已知字段后通过 entry_size 跳过未识别字节，不会错位。
    */

    struct ManifestHeader
    {
        ManifestHeader() noexcept = default;
        ManifestHeader(uint32_t magic, uint32_t version, uint32_t slang_version_size_in_bytes, uint32_t entry_count) noexcept :
            m_magic(magic),
            m_version(version),
            m_slang_version_size_in_bytes(slang_version_size_in_bytes),
            m_entry_count(entry_count) {}

        uint32_t m_magic = 0;
        uint32_t m_version = 0;
        uint32_t m_slang_version_size_in_bytes = 0;
        uint32_t m_entry_count = 0;
    };

    struct EntryHeader
    {
        EntryHeader() noexcept = default;
        EntryHeader(
            uint64_t entry_size_in_bytes,
            uint32_t source_path_size_in_bytes,
            uint32_t dep_count,
            uint32_t product_count,
            uint8_t profile_raw,
            uint32_t options_raw) noexcept :
            m_entry_size_in_bytes(entry_size_in_bytes) ,
            m_source_path_size_in_bytes(source_path_size_in_bytes),
            m_dep_count(dep_count),
            m_product_count(product_count),
            m_profile_raw(profile_raw),
            m_options_raw(options_raw) {}

        const uint64_t & getEntrySizeInBytes() const noexcept { return m_entry_size_in_bytes; }
        const uint32_t & getSourcePathSizeInBytes() const noexcept { return m_source_path_size_in_bytes; }
        const uint32_t & getDependencyCount() const noexcept { return m_dep_count; }
        const uint32_t & getProductCount() const noexcept { return m_product_count; }
        sl::TargetProfile getSlangTargetProfile() const noexcept { return static_cast<sl::TargetProfile>(m_profile_raw); }
        sl::CompilerOptionFlags getSlangCompilerOptions() const noexcept { return static_cast<sl::CompilerOptionFlags>(m_options_raw); }

        uint64_t m_entry_size_in_bytes = 0;
        uint32_t m_source_path_size_in_bytes = 0;
        uint32_t m_dep_count = 0;
        uint32_t m_product_count = 0;
        uint8_t  m_profile_raw = 0;
        uint8_t  m_reserved0 = 0;
        uint8_t  m_reserved1 = 0;
        uint8_t  m_reserved2 = 0;
        uint32_t m_options_raw = 0;
        uint32_t m_reserved_tail = 0; // padding to (alignof(EntryHeader) == 8)
    };

    struct DependencyHeader
    {
        DependencyHeader() noexcept = default;
        DependencyHeader(uint32_t dep_path_size_in_bytes, ShaderFingerprint fingerprint) noexcept :
            m_dep_path_size_in_bytes(dep_path_size_in_bytes), m_fingerprint(fingerprint) {}

        const uint32_t & getDependencyPathSizeInBytes() const noexcept { return m_dep_path_size_in_bytes; }
        const ShaderFingerprint & getFingerprint() const noexcept { return m_fingerprint; }

        uint32_t m_dep_path_size_in_bytes = 0;
        uint32_t m_reserved = 0; // padding to (alignof(DependencyHeader) == 8)
        ShaderFingerprint m_fingerprint;
    };

    struct ProductHeader
    {
        ProductHeader() noexcept = default;
        ProductHeader(uint32_t variant_key_size, uint64_t compile_input_hash) noexcept :
            m_variant_key_size(variant_key_size),
            m_compile_input_hash(compile_input_hash) {}
        
        const uint64_t & getCompileInputHash() const noexcept { return m_compile_input_hash; }

        uint32_t m_variant_key_size = 0;
        uint32_t m_reserved = 0; // padding to (alignof(ProductHeader) == 8)
        uint64_t m_compile_input_hash = 0;
    };
}

static stdfs::path manifest_file_path(const stdfs::path & work_dir) noexcept
{
    return work_dir / k_manifest_file_name;
}

static stdfs::path cache_path_for_hash(const stdfs::path & work_dir, uint64_t hash) noexcept
{
    return work_dir / std::format("{:016x}.spvbin", hash);
}

static std::string current_slang_global_version() noexcept
{
    const char * tag = ::spGetBuildTagString();
    return tag ? std::string(tag) : std::string("unknown");
}

static uint64_t mtime_to_u64(stdfs::file_time_type t) noexcept
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
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

static uint64_t compute_entry_payload_size(const ManifestEntry & entry) noexcept
{
    uint64_t size = entry.getMainRecord().getPath().string().size() + sizeof(ShaderFingerprint);
    for (const auto & dep : entry.getDependencyRecords()) {
        size += sizeof(DependencyHeader) + dep.getPath().string().size();
    }
    for (const auto & prod : entry.getProducts()) {
        size += sizeof(ProductHeader) + prod.m_variant_key.size();
    }
    return size;
}


static bool read_manifest_entry(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & out_entry) noexcept;

static void remove_cache_garbage(const stdfs::path & cache_dir, const std::unordered_set<uint64_t> & live_hashes) noexcept;

static ManifestEntryMap read_manifest_from_disk(const stdfs::path & work_dir, std::string_view current_slang_version) noexcept;

static std::error_code write_manifest_to_disk(const stdfs::path & work_dir, const ManifestEntryMap & entries, std::string_view slang_version) noexcept;

// =====================================================================
// ShaderFingerprint / Manifest (namespace lcf::shader_core)
// =====================================================================

ShaderFingerprint::ShaderFingerprint(const stdfs::path & path) noexcept
{
    std::error_code ec;
    if (not stdfs::exists(path, ec) or ec) { return; }
    auto file_size = stdfs::file_size(path, ec);
    if (ec) { return; }
    m_file_size = static_cast<uint64_t>(file_size);
    auto mt = stdfs::last_write_time(path, ec);
    if (not ec) { m_mtime = mtime_to_u64(mt); }
    m_content_hash = hash_file_content(path);
}

bool ShaderFingerprint::matches(const stdfs::path & path) const noexcept
{
    std::error_code ec;
    if (not stdfs::exists(path, ec) or ec) { return false; }
    auto file_size = stdfs::file_size(path, ec);
    if (ec or m_file_size != static_cast<uint64_t>(file_size)) { return false; }
    auto mtime = stdfs::last_write_time(path, ec);
    if (ec) { return false; }
    uint64_t mtime_u64 = mtime_to_u64(mtime);
    if (not ec and m_mtime == mtime_u64) { return true; }
    if (hash_file_content(path) != m_content_hash) { return false; }
    m_mtime = mtime_u64;
    return true;
}

Manifest::Manifest(stdfs::path work_dir) noexcept :
    m_work_dir(std::move(work_dir)),
    m_loaded_slang_global_version(current_slang_global_version()),
    m_entries(read_manifest_from_disk(manifest_file_path(m_work_dir), m_loaded_slang_global_version))
{
}

Manifest::~Manifest() noexcept
{
    this->shutdown();
}

const ManifestEntry * Manifest::find(const stdfs::path & source_path) noexcept
{
    auto canonical = normalize_manifest_path(source_path);
    auto it = m_entries.find(canonical);
    if (it == m_entries.end()) { return nullptr; }
    return &it->second;
}

void Manifest::upsert(ManifestEntry entry) noexcept
{
    auto key = entry.getMainRecord().getPath();
    m_pending_orphan_hashes.insert_range(
        m_entries[key].getProducts() |
        stdv::transform([](const auto & prod) { return prod.m_compile_input_hash; })
    );
    m_entries[key] = std::move(entry);
    m_dirty = true;
}

std::error_code Manifest::flush() noexcept
{
    if (not m_dirty) { return {}; }
    auto ec = write_manifest_to_disk(m_work_dir, m_entries, current_slang_global_version());
    if (not ec) { m_dirty = false; }
    return ec;
}

std::error_code Manifest::shutdown() noexcept
{
    std::unordered_set<uint64_t> live_hashes;
    for (const auto & [_, entry] : m_entries) {
        live_hashes.insert_range(entry.getProducts() |
            stdv::transform([](const auto & prod) { return prod.m_compile_input_hash; }));
    }
    for (auto hash : m_pending_orphan_hashes) {
        if (live_hashes.contains(hash)) { continue; }
        stdfs::remove(cache_path_for_hash(m_work_dir, hash));
    }
    m_pending_orphan_hashes.clear();

    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code ec;
    std::size_t file_count = std::distance(stdfs::directory_iterator(cache_dir, ec), {});
    if (not ec and file_count > live_hashes.size() + 1u) { remove_cache_garbage(cache_dir, live_hashes); }
    return this->flush();
}

FileRecord::FileRecord(stdfs::path path) noexcept :
    m_path(std::move(path)),
    m_path_hash(stdfs::hash_value(m_path)),
    m_fingerprint(m_path)
{
}

FileRecord::FileRecord(stdfs::path path, const ShaderFingerprint &fingerprint) noexcept :
    m_path(std::move(path)),
    m_path_hash(stdfs::hash_value(m_path)),
    m_fingerprint(fingerprint)
{
}

ManifestEntry::ManifestEntry(const stdfs::path & source_path) noexcept :
    m_main_record(normalize_manifest_path(source_path)),
    m_compile_settings(Config::instance().getSlangConfig().getCompileSettings())
{
}

ManifestEntry & ManifestEntry::addDependency(const stdfs::path & dependency_path)
{
    std::error_code ec;
    auto canonical_dependency_path = stdfs::weakly_canonical(dependency_path, ec);
    if (ec) { canonical_dependency_path = dependency_path; }
    FileRecord candidate(std::move(canonical_dependency_path));
    if (candidate.isSamePath(m_main_record)) { return *this; }
    for (const auto & existing : m_dependency_records) {
        if (existing.isSamePath(candidate)) { return *this; }
    }
    m_dependency_records.emplace_back(std::move(candidate));
    return *this;
}

ManifestEntry & ManifestEntry::addDependencyRecord(FileRecord record)
{
    m_dependency_records.emplace_back(std::move(record));
    return *this;
}

ManifestEntry & ManifestEntry::addProduct(const ProductRef & product) noexcept
{
    m_products.emplace_back(product);
    return *this;
}

ManifestEntry & ManifestEntry::setMainRecord(FileRecord record) noexcept
{
    m_main_record = std::move(record);
    return *this;
}

ManifestEntry & ManifestEntry::setCompileSettings(const sl::CompileSettings & settings) noexcept
{
    m_compile_settings = settings;
    return *this;
}

bool ManifestEntry::isOutdated() const noexcept
{
    if (m_compile_settings != Config::instance().getSlangConfig().getCompileSettings()) { return true; }
    if (m_main_record.isOutdated()) { return true; }
    if (m_products.empty()) { return true; }
    return std::ranges::any_of(m_dependency_records, [](const auto & dep) { return dep.isOutdated(); });
}
// =====================================================================
// long helper bodies
// =====================================================================

static bool read_manifest_entry(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & entry) noexcept
{
    std::string source_path_str(entry_header.getSourcePathSizeInBytes(), '\0');
    if (not reader.readBytes(as_bytes(source_path_str))) { return false; }

    ShaderFingerprint main_fingerprint;
    if (not reader.read(main_fingerprint)) { return false; }
    entry.setMainRecord(FileRecord{stdfs::path(source_path_str), main_fingerprint});

    for (uint32_t i = 0; i < entry_header.getDependencyCount(); ++i) {
        DependencyHeader dep_header;
        if (not reader.read(dep_header)) { return false; }
        std::string dep_path_str(dep_header.getDependencyPathSizeInBytes(), '\0');
        if (not reader.readBytes(as_bytes(dep_path_str))) { return false; }
        entry.addDependencyRecord(FileRecord{stdfs::path(dep_path_str), dep_header.getFingerprint()});
    }

    sl::CompileSettings settings;
    settings.setTargetProfile(entry_header.getSlangTargetProfile())
        .setCompilerOptionFlags(entry_header.getSlangCompilerOptions());
    entry.setCompileSettings(settings);

    for (uint32_t p = 0; p < entry_header.m_product_count; ++p) {
        ProductHeader product_header;
        if (not reader.read(product_header)) { return false; }
        ProductRef prod;
        prod.m_variant_key.resize(product_header.m_variant_key_size);
        if (not reader.readBytes(as_bytes(prod.m_variant_key))) { return false; }
        prod.m_compile_input_hash = product_header.m_compile_input_hash;
        entry.addProduct(prod);
    }
    return true;
}

static void remove_cache_garbage(const stdfs::path & cache_dir, const std::unordered_set<uint64_t> & live_hashes) noexcept
{
    std::error_code ec;
    if (not stdfs::is_directory(cache_dir, ec) or ec) { return; }
    auto iter = stdfs::directory_iterator(cache_dir, ec), end = stdfs::directory_iterator{};
    for (; not ec and iter != end; iter.increment(ec)) {
        const auto & directory = *iter;
        if (not directory.is_regular_file(ec) or ec) { ec.clear(); continue; }
        if (directory.path().extension() == k_manifest_file_name) { continue; }
        const auto stem = directory.path().stem().string();
        uint64_t hash = 0;
        try { hash = std::stoull(stem, nullptr, 16); }
        catch (...) { continue; }
        if (live_hashes.contains(hash)) { continue; }
        stdfs::remove(directory.path());
    }
}

static ManifestEntryMap read_manifest_from_disk( const stdfs::path & work_dir, std::string_view current_slang_version) noexcept
{
    auto path = manifest_file_path(work_dir);
    auto expected_bytes = read_file_as_bytes(path);
    if (not expected_bytes) { return {}; }
    BufferReader reader(expected_bytes.value());
    ManifestHeader header;
    if (not reader.read(header)) { return {}; }
    std::string slang_version(header.m_slang_version_size_in_bytes, '\0');
    if (not reader.readBytes(as_bytes(slang_version))) { return {}; }
    if (header.m_magic != k_magic or
        header.m_version != k_version or
        slang_version != current_slang_version) { return {}; }
    ManifestEntryMap entries;
    for (uint32_t i = 0; i < header.m_entry_count; ++i) {
        EntryHeader entry_header;
        if (not reader.read(entry_header)) { break; }
        size_t payload_start = reader.offset();
        ManifestEntry entry;
        if (not read_manifest_entry(reader, entry_header, entry)) { break; }
        size_t consumed = reader.offset() - payload_start;
        if (consumed > entry_header.getEntrySizeInBytes()) { break; }
        if (consumed < entry_header.getEntrySizeInBytes()) {
            if (not reader.skip(entry_header.m_entry_size_in_bytes - consumed)) { break; }
        }
        entries.insert_or_assign(entry.getMainRecord().getPath(), std::move(entry));
    }
    return entries;
}

static std::error_code write_manifest_to_disk(
    const stdfs::path & work_dir,
    const ManifestEntryMap & entries,
    std::string_view slang_version) noexcept
{
    std::error_code ec;
    stdfs::create_directories(work_dir, ec);
    if (ec) { return ec; }
    BufferWriter writer;
    writer.write(ManifestHeader{
        k_magic,
        k_version,
        static_cast<uint32_t>(slang_version.size()),
        static_cast<uint32_t>(entries.size())
    });
    writer.writeBytes(as_bytes(slang_version));
    for (const auto & [_, entry] : entries) {
        auto source_path_str = entry.getMainRecord().getPath().string();
        writer.write(EntryHeader{
            compute_entry_payload_size(entry),
            static_cast<uint32_t>(source_path_str.size()),
            static_cast<uint32_t>(entry.getDependencyRecords().size()),
            static_cast<uint32_t>(entry.getProducts().size()),
            static_cast<uint8_t>(entry.getCompileSettings().getTargetProfile()),
            static_cast<uint32_t>(entry.getCompileSettings().getCompilerOptionFlags())
        });
        writer.writeBytes(as_bytes(source_path_str));
        writer.write(entry.getMainRecord().getFingerprint());
        for (const auto & dep : entry.getDependencyRecords()) {
            auto dep_path_str = dep.getPath().string();
            writer.write(DependencyHeader{ static_cast<uint32_t>(dep_path_str.size()), dep.getFingerprint() });
            writer.writeBytes(as_bytes(dep_path_str));
        }
        for (const auto & prod : entry.getProducts()) {
            writer.write(ProductHeader{
                static_cast<uint32_t>(prod.m_variant_key.size()),
                prod.getCompileInputHash()
            });
            writer.writeBytes(as_bytes(prod.m_variant_key));
        }
    }

    return write_file(manifest_file_path(work_dir), as_bytes(writer.getBuffer()));
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
