#include "shader_core/Manifest.h"
#include "shader_core/buffer_io.h"
#include "shader_core/config.h"
#include "shader_core/hash.h"
#include "file_utils.h"
#include "bytes.h"
#include "log.h"
#include <slang.h>
#include <chrono>
#include <ranges>

using namespace lcf;
using namespace lcf::shader_core;

namespace {
    constexpr uint32_t k_magic = 0x4C434D46; // "LCMF"
    constexpr uint32_t k_version = 1;

    std::filesystem::path manifest_file_path() noexcept
    {
        return Config::instance().getCacheDirectory() / "manifest.bin";
    }

    std::string current_slang_global_version() noexcept
    {
        const char * tag = ::spGetBuildTagString();
        return tag ? std::string(tag) : std::string("unknown");
    }

    // 把 file_clock 的 last_write_time 转换为自 epoch 起的纳秒数（uint64）。
    // 不在跨平台间比较——只在同一台机器上前后两次比较，所以直接 time_since_epoch().count() 即可。
    uint64_t mtime_to_u64(std::filesystem::file_time_type t) noexcept
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
    }

    // 计算单文件的 content_hash（封装"读取整文件 + 单 chunk 喂 shader_core::hash"）。
    // 读取失败返回 0——content_hash=0 与"未填充指纹"（默认构造）一致，调用方据此判 MISS。
    uint64_t hash_file_content(const std::filesystem::path & path) noexcept
    {
        auto bytes_or = read_file_as_bytes(path);
        if (not bytes_or) { return 0; }
        const auto bytes = as_const_bytes(bytes_or.value());
        return shader_core::hash(std::span<decltype(bytes)>(&bytes, 1));
    }

    // ---- 序列化辅助 ----

    void write_lstring(BufferWriter & w, std::string_view s) noexcept
    {
        uint32_t len = static_cast<uint32_t>(s.size());
        w.write(len);
        if (len > 0) {
            w.writeBytes(as_bytes_from_ptr(s.data(), s.size()));
        }
    }

    void write_path_lstring(BufferWriter & w, const std::filesystem::path & p) noexcept
    {
        write_lstring(w, std::string_view(p.string()));
    }

    void write_fingerprint(BufferWriter & w, const ShaderFingerprint & fingerprint) noexcept
    {
        w.write(fingerprint.m_mtime);
        w.write(fingerprint.m_file_size);
        w.write(fingerprint.m_content_hash);
    }

    bool read_lstring(BufferReader & r, std::string & out) noexcept
    {
        uint32_t len = 0;
        if (not r.read(len)) { return false; }
        out.assign(len, '\0');
        if (len > 0 and not r.readBytes(as_bytes(out))) { return false; }
        return true;
    }

    bool read_fingerprint(BufferReader & r, ShaderFingerprint & out) noexcept
    {
        return r.read(out.m_mtime) and r.read(out.m_file_size) and r.read(out.m_content_hash);
    }
}

namespace lcf::shader_core {

// ---------- ShaderFingerprint ----------

ShaderFingerprint::ShaderFingerprint(const std::filesystem::path & path) noexcept
{
    std::error_code ec;
    if (not std::filesystem::exists(path, ec) or ec) { return; }

    auto sz = std::filesystem::file_size(path, ec);
    if (ec) { return; }
    m_file_size = static_cast<uint64_t>(sz);

    auto mt = std::filesystem::last_write_time(path, ec);
    if (not ec) { m_mtime = mtime_to_u64(mt); }

    m_content_hash = hash_file_content(path);
}

bool ShaderFingerprint::matches(const std::filesystem::path & path) const noexcept
{
    std::error_code ec;
    if (not std::filesystem::exists(path, ec) or ec) { return false; }

    auto sz = std::filesystem::file_size(path, ec);
    if (ec or static_cast<uint64_t>(sz) != m_file_size) {
        // size 不等 → content_hash 也必然不等，直接判 false
        // 注意：不能仅凭 size 相等就判 true，因为 size 相同内容可能不同
        return false;
    }

    auto mt = std::filesystem::last_write_time(path, ec);
    if (not ec and mtime_to_u64(mt) == m_mtime) {
        return true;  // 快路径：mtime + size 都匹配
    }

    // 慢路径兜底：算 content_hash
    return hash_file_content(path) == m_content_hash;
}

// ---------- Manifest ----------

std::filesystem::path normalize_manifest_path(const std::filesystem::path & path) noexcept
{
    auto resolved = Config::instance().resolvePath(path);
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(resolved, ec);
    if (ec) {
        // 文件不一定存在（首次编译前可能拿到的是虚拟路径已解析但文件还没创建的情形）。
        // 退化为 absolute，再不行就原样返回。
        ec.clear();
        auto abs = std::filesystem::absolute(resolved, ec);
        return ec ? resolved : abs;
    }
    return canonical;
}

Manifest & Manifest::instance() noexcept
{
    static Manifest s_instance;
    return s_instance;
}

Manifest::~Manifest() noexcept
{
    if (m_dirty) {
        if (auto ec = writeToDisk()) {
            // 析构期不能抛，仅 log。flush 失败的代价是下次启动 MISS 一次重写——功能正确。
            lcf_log_warn("shader manifest flush on destruction failed: {}", ec.message());
        }
    }
}

const ManifestEntry * Manifest::find(const std::filesystem::path & resolved_source) noexcept
{
    this->ensureLoaded();
    auto it = m_entries.find(resolved_source);
    if (it == m_entries.end()) { return nullptr; }
    return &it->second;
}

void Manifest::upsert(ManifestEntry entry) noexcept
{
    this->ensureLoaded();
    auto key = entry.m_source_path;
    m_entries.insert_or_assign(std::move(key), std::move(entry));
    m_dirty = true;
}

std::error_code Manifest::flush() noexcept
{
    if (not m_dirty) { return {}; }
    auto ec = this->writeToDisk();
    if (not ec) { m_dirty = false; }
    return ec;
}

void Manifest::ensureLoaded() noexcept
{
    if (m_loaded) { return; }
    m_loaded = true;
    this->loadFromDisk();
}

void Manifest::loadFromDisk() noexcept
{
    auto path = manifest_file_path();
    auto bytes_or = read_file_as_bytes(path);
    if (not bytes_or) {
        // 不存在视作空 manifest，正常情况
        return;
    }
    const auto & data = bytes_or.value();
    BufferReader reader(data);

    uint32_t magic = 0, version = 0;
    if (not reader.read(magic) or magic != k_magic) {
        lcf_log_warn("shader manifest magic mismatch, discarding");
        return;
    }
    if (not reader.read(version) or version != k_version) {
        lcf_log_warn("shader manifest version mismatch (expected {}, got {}), discarding", k_version, version);
        return;
    }

    std::string slang_ver;
    if (not read_lstring(reader, slang_ver)) {
        lcf_log_warn("shader manifest header truncated, discarding");
        return;
    }
    auto current_ver = current_slang_global_version();
    if (slang_ver != current_ver) {
        lcf_log_info("slang version changed ({} -> {}), invalidating entire manifest", slang_ver, current_ver);
        return;  // 整 manifest 作废
    }
    m_loaded_slang_global_version = std::move(slang_ver);

    uint32_t entry_count = 0;
    if (not reader.read(entry_count)) {
        lcf_log_warn("shader manifest entry_count truncated, discarding");
        return;
    }

    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t entry_size = 0;
        if (not reader.read(entry_size)) {
            lcf_log_warn("shader manifest truncated at entry {}, abandoning rest", i);
            m_entries.clear();
            return;
        }
        size_t entry_start = reader.offset();

        // 单条 entry 的解析放在 lambda 里，任何 truncation 都返回 false → 整 manifest 作废。
        auto parse_entry = [&](ManifestEntry & entry) -> bool {
            std::string source_path_str;
            if (not read_lstring(reader, source_path_str)) { return false; }
            entry.m_source_path = std::filesystem::path(source_path_str);

            if (not read_fingerprint(reader, entry.m_main_fingerprint)) { return false; }

            uint32_t dep_count = 0;
            if (not reader.read(dep_count)) { return false; }
            entry.m_deps.reserve(dep_count);
            for (uint32_t d = 0; d < dep_count; ++d) {
                std::string dep_path_str;
                if (not read_lstring(reader, dep_path_str)) { return false; }
                ShaderFingerprint dep_fingerprint{};
                if (not read_fingerprint(reader, dep_fingerprint)) { return false; }
                entry.m_deps.emplace_back(std::filesystem::path(dep_path_str), dep_fingerprint);
            }

            uint8_t profile_raw = 0;
            if (not reader.read(profile_raw)) { return false; }
            entry.m_compile_settings.setTargetProfile(static_cast<slang::TargetProfile>(profile_raw));

            uint32_t options_raw = 0;
            if (not reader.read(options_raw)) { return false; }
            entry.m_compile_settings.setCompilerOptionFlags(static_cast<slang::CompilerOptionFlags>(options_raw));

            uint32_t product_count = 0;
            if (not reader.read(product_count)) { return false; }
            entry.m_products.reserve(product_count);
            for (uint32_t p = 0; p < product_count; ++p) {
                ProductRef prod;
                uint32_t key_size = 0;
                if (not reader.read(key_size)) { return false; }
                prod.m_variant_key.resize(key_size);
                if (key_size > 0 and not reader.readBytes(std::span<std::byte>(prod.m_variant_key))) { return false; }
                if (not reader.read(prod.m_compile_input_hash)) { return false; }
                entry.m_products.push_back(std::move(prod));
            }
            return true;
        };

        ManifestEntry entry;
        if (not parse_entry(entry)) {
            lcf_log_warn("shader manifest truncated inside entry {}, discarding rest", i);
            m_entries.clear();
            return;
        }

        // 跳过未识别字段（前向兼容）：reader 当前位置 - entry_start 应该 <= entry_size。
        size_t consumed = reader.offset() - entry_start;
        if (consumed < entry_size) {
            if (not reader.skip(entry_size - consumed)) {
                lcf_log_warn("shader manifest truncated inside entry {} skip, discarding rest", i);
                m_entries.clear();
                return;
            }
        } else if (consumed > entry_size) {
            lcf_log_warn("shader manifest entry {} over-read ({} > {}), discarding rest", i, consumed, entry_size);
            m_entries.clear();
            return;
        }

        m_entries.insert_or_assign(entry.m_source_path, std::move(entry));
    }
}

std::error_code Manifest::writeToDisk() const
{
    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) { return ec; }

    BufferWriter w;
    w.write(k_magic);
    w.write(k_version);
    write_lstring(w, std::string_view(current_slang_global_version()));
    uint32_t entry_count = static_cast<uint32_t>(m_entries.size());
    w.write(entry_count);

    for (const auto & [_, entry] : m_entries) {
        size_t size_slot = w.reserveSlot<uint32_t>();
        size_t entry_start = w.size();

        write_path_lstring(w, entry.m_source_path);
        write_fingerprint(w, entry.m_main_fingerprint);

        uint32_t dep_count = static_cast<uint32_t>(entry.m_deps.size());
        w.write(dep_count);
        for (const auto & [dep_path, dep_fingerprint] : entry.m_deps) {
            write_path_lstring(w, dep_path);
            write_fingerprint(w, dep_fingerprint);
        }

        uint8_t profile_raw = static_cast<uint8_t>(entry.m_compile_settings.getTargetProfile());
        w.write(profile_raw);
        uint32_t options_raw = static_cast<uint32_t>(entry.m_compile_settings.getCompilerOptionFlags());
        w.write(options_raw);

        uint32_t product_count = static_cast<uint32_t>(entry.m_products.size());
        w.write(product_count);
        for (const auto & prod : entry.m_products) {
            uint32_t key_size = static_cast<uint32_t>(prod.m_variant_key.size());
            w.write(key_size);
            if (key_size > 0) {
                w.writeBytes(std::span<const std::byte>(prod.m_variant_key));
            }
            w.write(prod.m_compile_input_hash);
        }

        uint32_t entry_size = static_cast<uint32_t>(w.size() - entry_start);
        w.patch(size_slot, entry_size);
    }

    return write_file(manifest_file_path(), std::span<const std::byte>(w.buffer()));
}

} // namespace lcf::shader_core
