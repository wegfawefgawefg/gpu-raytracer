#pragma once

#include <string_view>

#include "scene_data.h"

SceneData BuildSampleScene(
    std::string_view meshPath,
    std::string_view texturePath
);
