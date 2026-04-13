#pragma once

#include "Vulkan/vulkan_enums.h"
#include "Vulkan/vulkan_fwd_decls.h"
#include "resource_utils.h"
#include "SequentialIdAllocator.h"
#include "enums/enum_count.h"
#include <vulkan/vulkan.hpp>
#include <array>
#include <tsl/robin_map.h>

namespace lcf::render {
    class VulkanBindlessTextureIdTable
    {
        using Self = VulkanBindlessTextureIdTable;
        using Id = uint32_t;
        using IdAllocator = SequentialIdAllocator<Id>;
        using TexturePtrToIdMap = tsl::robin_map<VkImage, Id>;
        using IdTable = std::array<std::pair<IdAllocator, TexturePtrToIdMap>, enum_count_v<vkenums::BindlessTextureBinding>>;
    public:
        VulkanBindlessTextureIdTable() = default;
        ~VulkanBindlessTextureIdTable() noexcept = default;
        VulkanBindlessTextureIdTable(const Self &) = delete;
        VulkanBindlessTextureIdTable &operator=(const Self &) = delete;
        VulkanBindlessTextureIdTable(Self &&) = default;
        VulkanBindlessTextureIdTable &operator=(Self &&) = default;
    public:
        const Id & registerTexture(vkenums::BindlessTextureBinding binding, vk::Image texture_handle)
        {
            auto & [id_allocator, texture_to_id_map] = m_table[std::to_underlying(binding)];
            if (texture_to_id_map.contains(texture_handle)) { return texture_to_id_map.at(texture_handle); }
            auto id = id_allocator.allocate();
            return texture_to_id_map[texture_handle] = id;
        }
        void unregisterTexture(vkenums::BindlessTextureBinding binding, vk::Image texture_handle) noexcept
        {
            auto & [id_allocator, texture_to_id_map] = m_table[std::to_underlying(binding)];
            auto it = texture_to_id_map.find(texture_handle);
            if (it == texture_to_id_map.end()) { return; }
            auto id = it->second;
            texture_to_id_map.erase(it);
            id_allocator.deallocate(id);
        }
        bool containsTexture(vkenums::BindlessTextureBinding binding, vk::Image texture_handle) const noexcept
        {
            return m_table[std::to_underlying(binding)].second.contains(texture_handle);
        }
        const Id & getTextureId(vkenums::BindlessTextureBinding binding, vk::Image texture_handle) const noexcept
        {
            const auto & [_, texture_to_id_map] = m_table[std::to_underlying(binding)];
            auto it = texture_to_id_map.find(texture_handle);
            if (it == texture_to_id_map.end()) {
                static Id s_default_id = 0;
                return s_default_id;
            }
            return it->second;
        }
    private:
        IdTable m_table;
    };

    class VulkanTextureManager
    {
    public:
    private:
    };
}