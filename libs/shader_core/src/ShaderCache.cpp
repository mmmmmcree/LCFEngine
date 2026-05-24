#include "shader_core/ShaderCache.h"
#include "log.h"
#include <format>
#include <fstream>
#include <vector>

using namespace lcf;
using namespace lcf::shader_core;

namespace {
    constexpr uint32_t k_magic = 0x4C434653;
    constexpr uint32_t k_version = 1;

    template <typename T>
    bool read_pod(std::istream & is, T & out) noexcept
    {
        is.read(reinterpret_cast<char *>(&out), sizeof(T));
        return is.good();
    }

    template <typename T>
    void write_pod(std::ostream & os, const T & in) noexcept
    {
        os.write(reinterpret_cast<const char *>(&in), sizeof(T));
    }

    std::filesystem::path make_cache_entry_path(const std::filesystem::path & cache_dir, uint64_t hash) noexcept
    {
        return cache_dir / std::format("{:016x}.spvbin", hash);
    }
}

ShaderCache::ShaderCache(std::filesystem::path cache_dir) : m_cache_dir(std::move(cache_dir))
{
    std::error_code ec;
    std::filesystem::create_directories(m_cache_dir, ec);
    if (ec) {
        lcf_log_warn("ShaderCache: failed to create cache directory '{}': {}", m_cache_dir.string(), ec.message());
    }
}

std::optional<spirv::UnitList> ShaderCache::tryLoad(uint64_t hash) const noexcept
{
    auto path = make_cache_entry_path(m_cache_dir, hash);
    std::ifstream file(path, std::ios::binary);
    if (not file) { return std::nullopt; }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t entry_count = 0;
    if (not read_pod(file, magic) or magic != k_magic) { return std::nullopt; }
    if (not read_pod(file, version) or version != k_version) { return std::nullopt; }
    if (not read_pod(file, entry_count)) { return std::nullopt; }

    spirv::UnitList units;
    units.reserve(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t stage_raw = 0;
        uint32_t name_size = 0;
        uint32_t spv_word_count = 0;
        if (not read_pod(file, stage_raw)) { return std::nullopt; }
        if (not read_pod(file, name_size)) { return std::nullopt; }
        std::string name(name_size, '\0');
        if (name_size > 0) { file.read(name.data(), name_size); if (not file.good()) { return std::nullopt; } }
        if (not read_pod(file, spv_word_count)) { return std::nullopt; }
        spirv::Code code(spv_word_count);
        if (spv_word_count > 0) {
            file.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(spv_word_count) * sizeof(uint32_t));
            if (not file.good()) { return std::nullopt; }
        }
        units.emplace_back(static_cast<ShaderTypeFlagBits>(stage_raw), std::move(code), std::move(name));
    }
    return units;
}

bool ShaderCache::store(uint64_t hash, const spirv::UnitList & units) const noexcept
{
    auto path = make_cache_entry_path(m_cache_dir, hash);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (not file) {
        lcf_log_warn("ShaderCache: failed to open cache file '{}' for writing", path.string());
        return false;
    }
    write_pod(file, k_magic);
    write_pod(file, k_version);
    write_pod(file, static_cast<uint32_t>(units.size()));
    for (const auto & unit : units) {
        const auto & name = unit.getEntryPoint();
        const auto & code = unit.getCode();
        write_pod(file, static_cast<uint32_t>(unit.getStage()));
        write_pod(file, static_cast<uint32_t>(name.size()));
        file.write(name.data(), static_cast<std::streamsize>(name.size()));
        write_pod(file, static_cast<uint32_t>(code.size()));
        file.write(reinterpret_cast<const char *>(code.data()), static_cast<std::streamsize>(code.size()) * sizeof(uint32_t));
    }
    if (not file.good()) {
        lcf_log_warn("ShaderCache: write failed for '{}'", path.string());
        return false;
    }
    return true;
}
