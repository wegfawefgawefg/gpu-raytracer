#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

constexpr std::uint32_t kOverlayBufferWidth = 512;
constexpr std::uint32_t kOverlayBufferHeight = 64;
constexpr std::uint32_t kOverlayPixelCount = kOverlayBufferWidth * kOverlayBufferHeight;

enum class MaterialKind : std::uint32_t
{
    Diffuse = 0,
    Emissive = 1,
    Mirror = 2,
};

struct alignas(16) GpuSphere
{
    Float4 centerRadius;
    Float4 albedoKind;
    Float4 emission;
};

struct alignas(16) GpuFrameParams
{
    Float4 cameraPosition;
    Float4 cameraForward;
    Float4 cameraRight;
    Float4 cameraUp;
    Float4 renderInfo;
    Float4 frameInfo;
    Float4 overlayInfo;
};

inline std::array<GpuSphere, 4> BuildDefaultScene()
{
    return {{
        {
            .centerRadius = {0.0f, -1001.0f, 0.0f, 1000.0f},
            .albedoKind = {0.75f, 0.78f, 0.82f, static_cast<float>(MaterialKind::Diffuse)},
            .emission = {0.0f, 0.0f, 0.0f, 0.0f},
        },
        {
            .centerRadius = {1.15f, 0.05f, -0.25f, 1.0f},
            .albedoKind = {0.94f, 0.96f, 0.99f, static_cast<float>(MaterialKind::Mirror)},
            .emission = {0.0f, 0.0f, 0.0f, 0.0f},
        },
        {
            .centerRadius = {-1.15f, 0.05f, -0.25f, 1.0f},
            .albedoKind = {0.94f, 0.96f, 0.99f, static_cast<float>(MaterialKind::Mirror)},
            .emission = {0.0f, 0.0f, 0.0f, 0.0f},
        },
        {
            .centerRadius = {1.6f, 3.0f, 1.0f, 0.45f},
            .albedoKind = {1.0f, 0.98f, 0.95f, static_cast<float>(MaterialKind::Emissive)},
            .emission = {18.0f, 17.0f, 15.0f, 0.0f},
        },
    }};
}
