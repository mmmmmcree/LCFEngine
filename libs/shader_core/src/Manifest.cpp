#include "shader_core/Manifest.h"
#include "shader_core/buffer_io.h"
#include "shader_core/config.h"
#include "shader_core/hash.h"
#include "file_utils.h"
#include "bytes.h"
#include <slang.h>
#include <chrono>
#include <ranges>

using namespace lcf;
using namespace lcf::shader_core;

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

static std::filesystem::path manifest_file_path() noexcept
{
    return Config::instance().getCacheDirectory() / "manifest.bin";
}

static std::string current_slang_global_version() noexcept
{
    const char * tag = ::spGetBuildTagString();
    return tag ? std::string(tag) : std::string("unknown");
}

static uint64_t mtime_to_u64(std::filesystem::file_time_type t) noexcept
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
}

static uint64_t hash_file_content(const std::filesystem::path & path) noexcept
{
    auto expected_bytes = read_file_as_bytes(path);
    if (not expected_bytes) { return 0; }
    return shader_core::hash(as_const_bytes(expected_bytes.value()));
}

static uint64_t compute_entry_payload_size(const ManifestEntry & entry) noexcept
{
    uint64_t size = entry.m_source_path.string().size() + sizeof(ShaderFingerprint);
    for (const auto & [dep_path, _] : entry.m_deps) {
        size += sizeof(DependencyHeader) + dep_path.string().size();
    }
    for (const auto & prod : entry.m_products) {
        size += sizeof(ProductHeader) + prod.m_variant_key.size();
    }
    return size;
}

static bool parse_entry_payload(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & out_entry) noexcept;

static ManifestEntryMap read_manifest_from_disk(const std::filesystem::path & path, std::string_view current_slang_version) noexcept;

static std::error_code write_manifest_to_disk(const std::filesystem::path & path, const ManifestEntryMap & entries, std::string_view slang_version) noexcept;

// =====================================================================
// ShaderFingerprint / Manifest (namespace lcf::shader_core)
// =====================================================================

namespace lcf::shader_core {

ShaderFingerprint::ShaderFingerprint(const std::filesystem::path & path) noexcept
{
    std::error_code ec;
    if (not std::filesystem::exists(path, ec) or ec) { return; }
    auto file_size = std::filesystem::file_size(path, ec);
    if (ec) { return; }
    m_file_size = static_cast<uint64_t>(file_size);
    auto mt = std::filesystem::last_write_time(path, ec);
    if (not ec) { m_mtime = mtime_to_u64(mt); }
    m_content_hash = hash_file_content(path);
}

bool ShaderFingerprint::matches(const std::filesystem::path & path) const noexcept
{
    std::error_code ec;
    if (not std::filesystem::exists(path, ec) or ec) { return false; }
    auto file_size = std::filesystem::file_size(path, ec);
    if (ec or m_file_size != static_cast<uint64_t>(file_size)) { return false; }
    auto mtime = std::filesystem::last_write_time(path, ec);
    if (not ec and m_mtime == mtime_to_u64(mtime)) { return true; }
    return hash_file_content(path) == m_content_hash;
}

std::filesystem::path normalize_manifest_path(const std::filesystem::path & path) noexcept
{
    auto resolved_path = Config::instance().resolvePath(path);
    std::error_code ec;
    auto canonical_path = std::filesystem::weakly_canonical(resolved_path, ec);
    if (ec) {
        auto abs_path = std::filesystem::absolute(resolved_path, ec);
        return ec ? resolved_path : abs_path;
    }
    return canonical_path;
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
    if (not m_dirty) { return; }
    write_manifest_to_disk(manifest_file_path(), m_entries, current_slang_global_version());
}

const ManifestEntry * Manifest::find(const std::filesystem::path & resolved_source) noexcept
{
    auto it = m_entries.find(resolved_source);
    if (it == m_entries.end()) { return nullptr; }
    return &it->second;
}

void Manifest::upsert(ManifestEntry entry) noexcept
{
    auto key = entry.m_source_path;
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

} // namespace lcf::shader_core

// =====================================================================
// long helper bodies
// =====================================================================

static bool parse_entry_payload(BufferReader & reader, const EntryHeader & entry_header, ManifestEntry & entry) noexcept
{
    std::string source_path_str(entry_header.m_source_path_size, '\0');
    if (not reader.readBytes(as_bytes(source_path_str))) { return false; }
    entry.m_source_path = std::filesystem::path(source_path_str);

    if (not reader.read(entry.m_main_fingerprint)) { return false; }

    entry.m_deps.reserve(entry_header.m_dep_count);
    for (uint32_t d = 0; d < entry_header.m_dep_count; ++d) {
        DependencyHeader dep_header;
        if (not reader.read(dep_header)) { return false; }
        std::string dep_path_str(dep_header.m_dep_path_size, '\0');
        if (not reader.readBytes(as_bytes(dep_path_str))) { return false; }
        entry.m_deps.emplace_back(std::filesystem::path(dep_path_str), dep_header.m_fingerprint);
    }

    entry.m_compile_settings.setTargetProfile(static_cast<shader_core::slang::TargetProfile>(entry_header.m_profile_raw));
    entry.m_compile_settings.setCompilerOptionFlags(static_cast<shader_core::slang::CompilerOptionFlags>(entry_header.m_options_raw));

    entry.m_products.reserve(entry_header.m_product_count);
    for (uint32_t p = 0; p < entry_header.m_product_count; ++p) {
        ProductHeader product_header;
        if (not reader.read(product_header)) { return false; }
        ProductRef prod;
        prod.m_variant_key.resize(product_header.m_variant_key_size);
        if (not reader.readBytes(as_bytes(prod.m_variant_key))) { return false; }
        prod.m_compile_input_hash = product_header.m_compile_input_hash;
        entry.m_products.push_back(std::move(prod));
    }
    return true;
}

static ManifestEntryMap read_manifest_from_disk( const std::filesystem::path & path, std::string_view current_slang_version) noexcept
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
        entries.insert_or_assign(entry.m_source_path, std::move(entry));
    }
    return entries;
}

static std::error_code write_manifest_to_disk(
    const std::filesystem::path & path,
    const ManifestEntryMap & entries,
    std::string_view slang_version) noexcept
{
    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
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
        auto source_path_str = entry.m_source_path.string();
        writer.write(EntryHeader{
            compute_entry_payload_size(entry),
            static_cast<uint32_t>(source_path_str.size()),
            static_cast<uint32_t>(entry.m_deps.size()),
            static_cast<uint32_t>(entry.m_products.size()),
            static_cast<uint8_t>(entry.m_compile_settings.getTargetProfile()),
            static_cast<uint32_t>(entry.m_compile_settings.getCompilerOptionFlags())
        });
        writer.writeBytes(as_bytes(source_path_str));
        writer.write(entry.m_main_fingerprint);
        for (const auto & [dep_path, dep_fingerprint] : entry.m_deps) {
            auto dep_path_str = dep_path.string();
            writer.write(DependencyHeader{ static_cast<uint32_t>(dep_path_str.size()), dep_fingerprint });
            writer.writeBytes(as_bytes(dep_path_str));
        }
        for (const auto & prod : entry.m_products) {
            writer.write(ProductHeader{
                static_cast<uint32_t>(prod.m_variant_key.size()),
                prod.m_compile_input_hash
            });
            writer.writeBytes(as_bytes(prod.m_variant_key));
        }
    }

    return write_file(path, as_bytes(writer.getBuffer()));
}
