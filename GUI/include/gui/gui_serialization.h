#pragma once

#include "JSON.h"
#include "gui_fwd_decls.h"

namespace lcf {
    OrderedJSON serialize(const gui::DisplayModeInfo & display_mode_info);
    
    OrderedJSON serialize(const gui::DisplayerInfo & displayer_info);
}