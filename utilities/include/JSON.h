#pragma once

#include <nlohmann/json.hpp>

namespace lcf {
    using JSON = nlohmann::json;
    using OrderedJSON = nlohmann::ordered_json;
}