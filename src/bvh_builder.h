#pragma once

#include <vector>

#include "gpu_types.h"

std::vector<GpuBvhNode> BuildTriangleBvh(std::vector<GpuTriangle>& triangles);
