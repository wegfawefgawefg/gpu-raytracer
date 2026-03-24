#pragma once

#include <cstdint>
#include <vector>

#include "gpu_types.h"

struct SceneData
{
    std::vector<GpuSphere> spheres;
    std::vector<GpuTriangle> triangles;
    std::vector<GpuMaterial> materials;
    std::vector<GpuBvhNode> bvhNodes;
    std::vector<GpuTextureInfo> textures;
    std::vector<std::uint32_t> texturePixels;
};
