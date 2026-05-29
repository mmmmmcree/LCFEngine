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

using namespace lcf;
using namespace lcf::shader_core;
namespace stdfs = std::filesystem;

namespace {
    constexpr uint32_t k_magic = 0x4C434D46; // "LCMF"
    constexpr uint32_t k_version = 2;        // v2: header-first 紧凑布局（每个变长域前都先甩一个定长 header struct）

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
        ManifestHeader(uint32_t magic, uint32_t version, uint32_t slang_ver_size, uint32_t entry_count) noexcept :
            m_magic(magic), m_version(version), m_slang_ver_size(slang_ver_size), m_entry_count(entry_count) {}

        uint32_t m_magic = 0;
        uint32_t m_version = 0;
        uint32_t m_slang_ver_size = 0;
        uint32_t m_entry_count = 0;
    };

    struct EntryHeader
    {
        EntryHeader() noexcept = default;
        EntryHeader(
            uint64_t entry_size,
            uint32_t source_path_size,
            uint32_t dep_count,
            uint32_t product_count,
            uint8_t profile_raw,
            uint32_t options_raw) noexcept :
            m_entry_size_in_bytes(entry_size) ,
            m_source_path_size(source_path_size),
            m_dep_count(dep_count),
            m_product_count(product_count),
            m_profile_raw(profile_raw),
            m_options_raw(options_raw) {}

        uint64_t m_entry_size_in_bytes = 0;
        uint32_t m_source_path_size = 0;
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
        DependencyHeader(uint32_t dep_path_size, ShaderFingerprint fingerprint) noexcept :
            m_dep_path_size(dep_path_size), m_fingerprint(fingerprint) {}

        uint32_t m_dep_path_size = 0;
        uint32_t m_reserved = 0; // padding to (alignof(DependencyHeader) == 8)
        ShaderFingerprint m_fingerprint;
    };

    struct ProductHeader
    {
        ProductHeader() noexcept = default;
        ProductHeader(uint32_t variant_key_size, uint64_t compile_input_hash) noexcept :
            m_variant_key_size(variant_key_size),
            m_compile_input_hash(compile_input_hash) {}

        uint32_t m_variant_key_size = 0;
        uint32_t m_reserved = 0; // padding to (alignof(ProductHeader) == 8)
        uint64_t m_compile_input_hash = 0;
    };
}

static stdfs::path manifest_file_path() noexcept
{
    return Config::instance().getCacheDirectory() / "manifest.bin";
}

// 与 ShaderCache::make_cache_entry_path() 同步：cache_dir / "<16-hex>.spvbin"。
// 这里是 Manifest 的私有镜像，避免把 ShaderCache 的内部命名细节升级成公共 API；
// 一旦命名规则需要变化，两处必须同步修改（已在文档/comment 里强调）。
static stdfs::path cache_path_for_hash(uint64_t hash) noexcept
{
    return Config::instance().getCacheDirectory() / std::format("{:016x}.spvbin", hash);
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
    return shader_core::hash(as_const_bytes(expected_bytes.value()));
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

static bool parse_entry_payload(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & out_entry) noexcept;

static ManifestEntryMap read_manifest_from_disk(const stdfs::path & path, std::string_view current_slang_version) noexcept;

static std::error_code write_manifest_to_disk(const stdfs::path & path, const ManifestEntryMap & entries, std::string_view slang_version) noexcept;

// =====================================================================
// ShaderFingerprint / Manifest (namespace lcf::shader_core)
// =====================================================================

namespace lcf::shader_core {

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
    if (not ec and m_mtime == mtime_to_u64(mtime)) { return true; }
    return hash_file_content(path) == m_content_hash;
}

Manifest & Manifest::instance() noexcept
{
    static Manifest s_instance;
    return s_instance;
}

Manifest::Manifest() noexcept :
    m_loaded_slang_global_version(current_slang_global_version()),
    m_entries(read_manifest_from_disk(manifest_file_path(), m_loaded_slang_global_version))
{
}

Manifest::~Manifest() noexcept
{
    // 注意：在 DLL（Meyers-singleton）+ 某些 Windows toolchain（clang + WINDOWS_EXPORT_ALL_SYMBOLS）
    // 下，这个析构函数在进程退出路径上**不一定会被调用**。所以不要在这里放任何业务关键逻辑（GC、写盘等）—
    // 一切持久化都必须通过显式 shutdown()/flush() 完成。这里仅作 fallback：如果析构真的跑了且还有
    // 未 flush 的脏数据，尽力写一次盘以减少数据丢失。
    this->flush();
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

    // 增量记账：对比新旧 product，凡是被新版本"丢弃"的 hash，就把它推入待删队列。
    // shutdown() 删之前会再次用全量 live-hashes 二次过滤，避免误删——所以这里宽松入队即可。
    if (auto it = m_entries.find(key); it != m_entries.end()) {
        const auto & old_products = it->second.getProducts();
        const auto & new_products = entry.getProducts();
        for (const auto & old_prod : old_products) {
            bool still_alive_in_new = std::ranges::any_of(new_products, [&](const auto & np) {
                return np.m_compile_input_hash == old_prod.m_compile_input_hash;
            });
            if (not still_alive_in_new) {
                m_pending_orphan_hashes.insert(old_prod.m_compile_input_hash);
            }
        }
    }

    m_entries.insert_or_assign(std::move(key), std::move(entry));
    m_dirty = true;
}

std::error_code Manifest::flush() noexcept
{
    if (not m_dirty) { return {}; }
    auto ec = write_manifest_to_disk(manifest_file_path(), m_entries, current_slang_global_version());
    if (not ec) { m_dirty = false; }
    return ec;
}

std::error_code Manifest::shutdown() noexcept
{
    std::unordered_set<uint64_t> live_hashes;
    for (const auto & [_, entry] : m_entries) {
        for (const auto & prod : entry.getProducts()) {
            live_hashes.insert(prod.m_compile_input_hash);
        }
    }
    for (auto hash : m_pending_orphan_hashes) {
        if (live_hashes.contains(hash)) { continue; }
        std::error_code rm_ec;
        stdfs::remove(cache_path_for_hash(hash), rm_ec); // 失败忽略，不影响 flush。
    }
    m_pending_orphan_hashes.clear();

    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code dir_ec;
    if (stdfs::is_directory(cache_dir, dir_ec) and not dir_ec) {
        size_t disk_spvbin_count = 0;
        auto iter = stdfs::directory_iterator(cache_dir, dir_ec), end = stdfs::directory_iterator{};
        for (iter; not dir_ec and iter != end; iter.increment(dir_ec)) {
            if (iter->path().extension() == ".spvbin") { ++disk_spvbin_count; }
        }
        if (not dir_ec and disk_spvbin_count > live_hashes.size()) { this->removeGarbage(); }
    }

    return this->flush();
}

ManifestGcStats Manifest::removeGarbage() noexcept
{
    ManifestGcStats stats;

    // 阶段 1: 剔除 main source 已不存在的 manifest entry。
    // 注意：此处只清"路径都没了"的极端情况——单纯 outdated（mtime/hash 不同但文件还在）的 entry
    // 留到下一次实际编译时由 upsert() 自然覆盖；这样可以保留依赖图，避免 GC 误伤还在编辑中的文件。
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        const auto & main_path = it->second.getMainRecord().getPath();
        std::error_code ec;
        bool exists = stdfs::exists(main_path, ec);
        if (ec or not exists) {
            it = m_entries.erase(it);
            ++stats.m_entries_removed;
        } else {
            ++it;
        }
    }
    if (stats.m_entries_removed > 0) { m_dirty = true; }

    // 阶段 2: 收集还在被引用的 product hash 集合。
    std::unordered_set<uint64_t> live_hashes;
    for (const auto & [_, entry] : m_entries) {
        for (const auto & prod : entry.getProducts()) {
            live_hashes.insert(prod.m_compile_input_hash);
        }
    }

    // 阶段 3: 扫描 cache_dir，删除文件名匹配 `<16-hex>.spvbin` 但 hash 不在 live 集合中的孤立产物。
    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code dir_ec;
    if (not stdfs::is_directory(cache_dir, dir_ec) or dir_ec) { return stats; }

    for (auto iter = stdfs::directory_iterator(cache_dir, dir_ec);
         not dir_ec and iter != stdfs::directory_iterator{};
         iter.increment(dir_ec))
    {
        const auto & dirent = *iter;
        if (not dirent.is_regular_file(dir_ec) or dir_ec) { dir_ec.clear(); continue; }
        if (dirent.path().extension() != ".spvbin") { continue; }

        const auto stem = dirent.path().stem().string();
        if (stem.size() != 16) { continue; }
        if (not std::ranges::all_of(stem, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })) { continue; }

        uint64_t hash = 0;
        try { hash = std::stoull(stem, nullptr, 16); }
        catch (...) { continue; }

        if (live_hashes.contains(hash)) { continue; }

        std::error_code size_ec;
        auto file_size = stdfs::file_size(dirent.path(), size_ec);
        std::error_code rm_ec;
        if (stdfs::remove(dirent.path(), rm_ec) and not rm_ec) {
            ++stats.m_orphan_files_removed;
            if (not size_ec) { stats.m_bytes_reclaimed += static_cast<size_t>(file_size); }
        }
    }
    return stats;
}

} // namespace lcf::shader_core

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

ManifestEntry & ManifestEntry::setCompileSettings(const slang::CompileSettings & settings) noexcept
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

static bool parse_entry_payload(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & entry) noexcept
{
    std::string source_path_str(entry_header.m_source_path_size, '\0');
    if (not reader.readBytes(as_bytes(source_path_str))) { return false; }

    ShaderFingerprint main_fp;
    if (not reader.read(main_fp)) { return false; }
    entry.setMainRecord(FileRecord{stdfs::path(source_path_str), main_fp});

    for (uint32_t d = 0; d < entry_header.m_dep_count; ++d) {
        DependencyHeader dep_header;
        if (not reader.read(dep_header)) { return false; }
        std::string dep_path_str(dep_header.m_dep_path_size, '\0');
        if (not reader.readBytes(as_bytes(dep_path_str))) { return false; }
        entry.addDependencyRecord(FileRecord{stdfs::path(dep_path_str), dep_header.m_fingerprint});
    }

    shader_core::slang::CompileSettings settings;
    settings.setTargetProfile(static_cast<shader_core::slang::TargetProfile>(entry_header.m_profile_raw));
    settings.setCompilerOptionFlags(static_cast<shader_core::slang::CompilerOptionFlags>(entry_header.m_options_raw));
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

static ManifestEntryMap read_manifest_from_disk( const stdfs::path & path, std::string_view current_slang_version) noexcept
{
    auto expected_bytes = read_file_as_bytes(path);
    if (not expected_bytes) { return {}; }
    BufferReader reader(expected_bytes.value());
    ManifestHeader header;
    if (not reader.read(header)) { return {}; }
    std::string slang_version(header.m_slang_ver_size, '\0');
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
        if (not parse_entry_payload(reader, entry_header, entry)) { break; }
        size_t consumed = reader.offset() - payload_start;
        if (consumed > entry_header.m_entry_size_in_bytes) { break; }
        if (consumed < entry_header.m_entry_size_in_bytes) {
            if (not reader.skip(entry_header.m_entry_size_in_bytes - consumed)) { break; }
        }
        entries.insert_or_assign(entry.getMainRecord().getPath(), std::move(entry));
    }
    return entries;
}

static std::error_code write_manifest_to_disk(
    const stdfs::path & path,
    const ManifestEntryMap & entries,
    std::string_view slang_version) noexcept
{
    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code ec;
    stdfs::create_directories(cache_dir, ec);
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
                prod.m_compile_input_hash
            });
            writer.writeBytes(as_bytes(prod.m_variant_key));
        }
    }

    return write_file(path, as_bytes(writer.getBuffer()));
}
