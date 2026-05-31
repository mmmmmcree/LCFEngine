#include "shader_core/ShaderCache.h"
#include "shader_core/config.h"
#include "shader_core/buffer_io.h"
#include "shader_core/Manifest.h"
#include "file_utils.h"
#include "bytes.h"
#include "log.h"
#include <format>
#include <cstring>

using namespace lcf;
using namespace lcf::sc;
namespace stdfs = std::filesystem;

namespace {
    constexpr uint32_t k_magic = 0x4C434653;
    constexpr uint32_t k_version = 2;

    struct CacheHeader
    {
        CacheHeader() noexcept = default;
        CacheHeader(uint32_t magic, uint32_t version, uint64_t unit_count) noexcept :
            m_magic(magic),
            m_version(version),
            m_unit_count(unit_count) {}

        uint32_t m_magic = 0;
        uint32_t m_version = 0;
        uint64_t m_unit_count = 0;
    };

    struct SpvUnitHeader
    {
        SpvUnitHeader() noexcept = default;
        SpvUnitHeader(ShaderTypeFlagBits stage, uint64_t name_size_in_bytes, uint64_t spv_size_in_bytes) noexcept :
            m_stage(static_cast<uint32_t>(stage)),
            m_name_size_in_bytes(name_size_in_bytes),
            m_spv_size_in_bytes(spv_size_in_bytes) {}

        uint32_t m_stage = 0;
        uint32_t m_reserved = 0;
        uint64_t m_name_size_in_bytes = 0;
        uint64_t m_spv_size_in_bytes = 0;
    };

}

namespace lcf::sc::spirv {
    static const stdfs::path & get_cache_directory() noexcept
    {
        static const stdfs::path s_cache_dir = Config::instance().getCacheDirectory() / "spirv";
        return s_cache_dir;
    }

    static std::filesystem::path make_cache_entry_path(uint64_t hash) noexcept
    {
        return get_cache_directory() / std::format("{:016x}.spvbin", hash);
    }

    static Manifest & get_manifest_instance() noexcept
    {
        static Manifest s_manifest {get_cache_directory()};
        static bool s_initialized = [](Manifest & manifest) {
            ManifestManager::instance().registerStaticManifest(&manifest);
            return true;
        }(s_manifest);
        return s_manifest;
    }
}

std::optional<spirv::UnitList> spirv::ShaderCache::tryLoad(uint64_t hash) const noexcept
{
    auto path = make_cache_entry_path(hash);
    auto expected_data = read_file_as_bytes(path);
    if (not expected_data) { return std::nullopt; }
    const auto & data = expected_data.value();
    BufferReader reader {data};
    CacheHeader header;
    if (not reader.read(header)) { return std::nullopt; }
    if (header.m_magic != k_magic or header.m_version != k_version) { return std::nullopt; }
    spirv::UnitList units;
    units.reserve(header.m_unit_count);
    for (uint64_t i = 0; i < header.m_unit_count; ++i) {
        SpvUnitHeader unit_header;
        if (not reader.read(unit_header)) { return std::nullopt; }
        std::string name(unit_header.m_name_size_in_bytes, '\0');
        if (not reader.readBytes(as_bytes(name))) { return std::nullopt; }
        if (unit_header.m_spv_size_in_bytes % sizeof(uint32_t) != 0) { return std::nullopt; }
        spirv::Code code(unit_header.m_spv_size_in_bytes / sizeof(uint32_t));
        if (not reader.readBytes(as_bytes(code))) { return std::nullopt; }
        units.emplace_back(static_cast<ShaderTypeFlagBits>(unit_header.m_stage), std::move(code), std::move(name));
    }
    return units;
}

std::error_code spirv::ShaderCache::store(uint64_t hash, const spirv::UnitList & units) const noexcept
{
    BufferWriter writer;
    writer.write(CacheHeader{ k_magic, k_version, units.size() });
    for (const auto & unit : units) {
        auto name_bytes = as_bytes(unit.getEntryPoint());
        auto code_bytes = as_bytes(unit.getCode());
        writer.write(SpvUnitHeader{ unit.getStage(), name_bytes.size_bytes(), code_bytes.size_bytes() })
            .writeBytes(name_bytes)
            .writeBytes(code_bytes);
    }
    return write_file(make_cache_entry_path(hash), as_bytes(writer.getBuffer()));
}

std::optional<spirv::UnitList> spirv::ShaderCache::tryLoad(const std::filesystem::path &source_path) const noexcept
{
    auto & manifest = get_manifest_instance();
    const ManifestEntry * entry = manifest.find(source_path);
    if (not entry or entry->isOutdated()) { return std::nullopt; }
    return this->tryLoad(entry->getProducts().front().m_compile_input_hash);
}

void spirv::ShaderCache::store(const std::filesystem::path & source_path, const CompileResult &compile_result) const noexcept
{
    this->store(compile_result.getCacheHash(), compile_result.getUnits());

    ManifestEntry new_entry {source_path};
    new_entry.appendDependencies(compile_result.getDependencyPaths())
        .addProduct(sc::ProductRef{compile_result.getCacheHash()});
    auto & manifest = get_manifest_instance();
    manifest.upsert(std::move(new_entry));
    manifest.flush();
}