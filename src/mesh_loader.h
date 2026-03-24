#pragma once

#include <string_view>
#include <vector>

#include "gpu_types.h"
#include "math_types.h"

struct ObjMeshLoadParams
{
    std::string_view path;
    Float3 position;
    Float3 scale;
    Float3 rotationDegrees = {0.0f, 0.0f, 0.0f};
    bool normalize = true;
    Float3 albedo = {0.78f, 0.80f, 0.84f};
    MaterialKind materialKind = MaterialKind::Diffuse;
    Float3 emission = {0.0f, 0.0f, 0.0f};
};

std::vector<GpuTriangle> LoadObjTriangles(const ObjMeshLoadParams& params);
