#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma clang diagnostic pop

#include "texture_loader.h"

#include <stdexcept>
#include <string>

LoadedTexture LoadTextureRgba8(std::string_view path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* bytes =
        stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
    if (bytes == nullptr)
    {
        throw std::runtime_error("Failed to load texture: " + std::string(path));
    }

    LoadedTexture texture;
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.pixels.resize(texture.width * texture.height);

    for (std::size_t index = 0; index < texture.pixels.size(); ++index)
    {
        const std::size_t byteIndex = index * 4;
        texture.pixels[index] =
            static_cast<std::uint32_t>(bytes[byteIndex]) |
            (static_cast<std::uint32_t>(bytes[byteIndex + 1]) << 8u) |
            (static_cast<std::uint32_t>(bytes[byteIndex + 2]) << 16u) |
            (static_cast<std::uint32_t>(bytes[byteIndex + 3]) << 24u);
    }

    stbi_image_free(bytes);
    return texture;
}
