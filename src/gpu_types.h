#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

constexpr std::uint32_t kOverlayBufferWidth = 512;
constexpr std::uint32_t kOverlayBufferHeight = 64;
constexpr std::uint32_t kOverlayPixelCount = kOverlayBufferWidth * kOverlayBufferHeight;
constexpr std::uint32_t kInvalidTextureIndex = 0xFFFFFFFFu;

enum class MaterialKind : std::uint32_t
{
    Diffuse = 0,
    Emissive = 1,
    Mirror = 2,
};

struct alignas(16) GpuSphere
{
    Float4 centerRadius;
    UInt4 materialIndex;
};

struct alignas(16) GpuTriangle
{
    Float4 a;
    Float4 b;
    Float4 c;
    Float4 uvAB;
    Float4 uvCMaterial;
};

struct alignas(16) GpuMaterial
{
    Float4 albedoRoughness;
    Float4 emission;
    UInt4 meta;
};

struct alignas(16) GpuBvhNode
{
    Float4 boundsMin;
    Float4 boundsMax;
    UInt4 meta;
};

struct alignas(16) GpuTextureInfo
{
    UInt4 sizeOffset;
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
    Float4 sceneInfo;
};

inline std::array<GpuSphere, 3> BuildDefaultScene()
{
    return {{
        {
            .centerRadius = {0.0f, -1001.0f, 0.0f, 1000.0f},
            .materialIndex = {0, 0, 0, 0},
        },
        {
            .centerRadius = {1.45f, 0.10f, 0.35f, 0.90f},
            .materialIndex = {1, 0, 0, 0},
        },
        {
            .centerRadius = {0.0f, 2.8f, 1.4f, 0.50f},
            .materialIndex = {2, 0, 0, 0},
        },
    }};
}
