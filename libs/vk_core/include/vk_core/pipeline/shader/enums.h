#pragma once

#include <cstdint>
#include <utility>
#include <vulkan/vulkan_enums.hpp>
#include "enums/enum_traits.h"

template <>
struct lcf::enum_traits<vk::ShaderStageFlagBits>
{
private:
    using Stage = vk::ShaderStageFlagBits;
    using StageFlags = vk::ShaderStageFlags;
public:
    static constexpr StageFlags valid_next_stages_of(Stage stage) noexcept
    {
        switch (stage) {
            case Stage::eVertex:
                return Stage::eTessellationControl | Stage::eGeometry | Stage::eFragment;
            case Stage::eTessellationControl:
                return Stage::eTessellationEvaluation;
            case Stage::eTessellationEvaluation:
                return Stage::eGeometry | Stage::eFragment;
            case Stage::eGeometry:
            case Stage::eMeshEXT:
                return Stage::eFragment;
            case Stage::eTaskEXT:
                return Stage::eMeshEXT;
            default:
                return {};
        }
    }
    static constexpr bool is_valid_next_stage_of(Stage stage, StageFlags next_stages) noexcept
    {
        return (valid_next_stages_of(stage) & next_stages) == next_stages;
    }

    static constexpr uint32_t order_of(Stage stage) noexcept
    {
        switch (stage) {
            case Stage::eTaskEXT: return 0u;
            case Stage::eVertex: return 1u;
            case Stage::eTessellationControl: return 2u;
            case Stage::eTessellationEvaluation: return 3u;
            case Stage::eGeometry: return 4u;
            case Stage::eMeshEXT: return 5u;
            case Stage::eFragment: return 6u;
            case Stage::eCompute: return 7u;
            default: return 8u;
        }
    }

    static constexpr bool precedes_in_pipeline(Stage lhs, Stage rhs) noexcept
    {
        const auto lhs_order = order_of(lhs);
        const auto rhs_order = order_of(rhs);
        return lhs_order == rhs_order ? std::to_underlying(lhs) < std::to_underlying(rhs) : lhs_order < rhs_order;
    }

    struct pipeline_order_less_t
    {
        constexpr bool operator()(Stage lhs, Stage rhs) const noexcept
        {
            return precedes_in_pipeline(lhs, rhs);
        }
    };
};
