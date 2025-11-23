#pragma once

#include "JSON.h"
#include "gui_forward_declares.h"

namespace lcf {
    OrderedJSON serialize(const gui::DisplayModeInfo & display_mode_info);
    
    OrderedJSON serialize(const gui::DisplayerInfo & displayer_info);
}