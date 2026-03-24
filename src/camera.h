#pragma once

#include <cstdint>

#include "gpu_types.h"

struct Camera
{
    Camera();

    void UpdateLook(float deltaX, float deltaY);
    bool UpdateMovement(const bool* keys, float deltaSeconds);
    GpuFrameParams BuildFrameParams(
        std::uint32_t renderWidth,
        std::uint32_t renderHeight,
        std::uint32_t sphereCount,
        std::uint32_t triangleCount
    ) const;
    Float3 GetForward() const;
    Float3 GetRight() const;

    Float3 position;
    float yawDegrees = -90.0f;
    float pitchDegrees = 0.0f;
    float verticalFovDegrees = 60.0f;
    float moveSpeed = 4.0f;
    float lookSensitivity = 0.12f;
};
