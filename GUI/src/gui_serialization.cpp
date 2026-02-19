#include "gui/gui_serialization.h"
#include "gui/gui_types.h"

using namespace lcf;

OrderedJSON lcf::serialize(const gui::DisplayModeInfo &display_mode_info)
{
    return OrderedJSON 
    {
        {"type_name", "lcf::gui::DisplayModeInfo"},
        {"logical_width", display_mode_info.getLogicalWidth()},
        {"logical_height", display_mode_info.getLogicalHeight()},
        {"pixel_ratio", display_mode_info.getPixelRatio()},
        {"refresh_rate", display_mode_info.getRefreshRate()},
    };
}

OrderedJSON lcf::serialize(const gui::DisplayerInfo &displayer_info)
{
    OrderedJSON available_modes = OrderedJSON::array();
    for (const auto &mode : displayer_info.m_available_mode_infos) {
        available_modes.emplace_back(serialize(mode));
    }
    return OrderedJSON 
    {
        {"type_name", "lcf::gui::DisplayerInfo"},
        {"is_primary", displayer_info.m_is_primary},
        {"index", displayer_info.m_index},
        {"name", displayer_info.m_name},
        {"desktop_mode_info", serialize(displayer_info.m_desktop_mode_info)},
        {"current_mode_info", serialize(displayer_info.m_current_mode_info)},
        {"available_mode_infos", available_modes}
    };
}