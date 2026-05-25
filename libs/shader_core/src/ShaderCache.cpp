#include "shader_core/ShaderCache.h"
#include "shader_core/config.h"
#include "shader_core/buffer_io.h"
#include "file_utils.h"
#include "bytes.h"
#include "log.h"
#include <format>
#include <cstring>

using namespace lcf;
using namespace lcf::shader_core;

namespace {
    constexpr uint32_t k_magic = 0x4C434653;
    constexpr uint32_t k_version = 1;
    constexpr size_t k_header_size = sizeof(uint32_t) * 3; // magic + version + entry_count
    constexpr size_t k_entry_fixed_size = sizeof(uint32_t) * 3; // stage + name_size + spv_word_count

    std::filesystem::path make_cache_entry_path(uint64_t hash) noexcept
    {
        return Config::instance().getCacheDirectory() / std::format("{:016x}.spvbin", hash);
    }
}

std::optional<spirv::UnitList> ShaderCache::tryLoad(uint64_t hash) const noexcept
{
    auto path = make_cache_entry_path(hash);
    auto expected_data = read_file_as_bytes(path);
    if (not expected_data) { return std::nullopt; }
    const auto & data = expected_data.value();
    if (data.size() < k_header_size) { return std::nullopt; }

    BufferReader reader(data);
    uint32_t magic = 0, version = 0, entry_count = 0;
    if (not reader.read(magic) or magic != k_magic) { return std::nullopt; }
    if (not reader.read(version) or version != k_version) { return std::nullopt; }
    if (not reader.read(entry_count)) { return std::nullopt; }

    spirv::UnitList units;
    units.reserve(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t stage_raw = 0, name_size = 0, spv_word_count = 0;
        if (not reader.read(stage_raw)) { return std::nullopt; }
        if (not reader.read(name_size)) { return std::nullopt; }
        std::string name(name_size, '\0');
        if (name_size > 0 and not reader.readBytes(as_bytes(name))) { return std::nullopt; }
        if (not reader.read(spv_word_count)) { return std::nullopt; }
        spirv::Code code(spv_word_count);
        if (spv_word_count > 0 and not reader.readBytes(as_bytes(code))) { return std::nullopt; }
        units.emplace_back(static_cast<ShaderTypeFlagBits>(stage_raw), std::move(code), std::move(name));
    }
    return units;
}

std::error_code ShaderCache::store(uint64_t hash, const spirv::UnitList & units) const noexcept
{
    const auto & cache_dir = Config::instance().getCacheDirectory();
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) { return ec; }

    size_t total_size = k_header_size;
    for (const auto & unit : units) {
        total_size += k_entry_fixed_size + unit.getEntryPoint().size() + unit.getCode().size() * sizeof(uint32_t);
    }
    std::vector<std::byte> buffer(total_size);
    FixedBufferWriter writer(buffer);
    writer.writeBytes(as_bytes_from_value(k_magic));
    writer.writeBytes(as_bytes_from_value(k_version));
    uint32_t entry_count = static_cast<uint32_t>(units.size());
    writer.writeBytes(as_bytes_from_value(entry_count));
    for (const auto & unit : units) {
        const auto & name = unit.getEntryPoint();
        const auto & code = unit.getCode();
        uint32_t stage = static_cast<uint32_t>(unit.getStage());
        uint32_t name_size = static_cast<uint32_t>(name.size());
        uint32_t spv_word_count = static_cast<uint32_t>(code.size());
        writer.writeBytes(as_bytes_from_value(stage));
        writer.writeBytes(as_bytes_from_value(name_size));
        writer.writeBytes(as_bytes(name));
        writer.writeBytes(as_bytes_from_value(spv_word_count));
        writer.writeBytes(as_bytes(code));
    }
    return write_file(make_cache_entry_path(hash), buffer);
}
