#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

struct LoadedTexture
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> pixels;
};

LoadedTexture LoadTextureRgba8(std::string_view path);
