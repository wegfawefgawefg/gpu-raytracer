#include "bvh_builder.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace
{
constexpr std::uint32_t kLeafTriangleCount = 4;

struct TriangleRef
{
    GpuTriangle triangle;
    Float3 boundsMin;
    Float3 boundsMax;
    Float3 centroid;
};

struct Bounds
{
    Float3 min = {1e30f, 1e30f, 1e30f};
    Float3 max = {-1e30f, -1e30f, -1e30f};
};

Float3 Min(const Float3& a, const Float3& b)
{
    return {
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z),
    };
}

Float3 Max(const Float3& a, const Float3& b)
{
    return {
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z),
    };
}

Float3 ToFloat3(const Float4& value)
{
    return {value.x, value.y, value.z};
}

TriangleRef BuildRef(const GpuTriangle& triangle)
{
    const Float3 a = ToFloat3(triangle.a);
    const Float3 b = ToFloat3(triangle.b);
    const Float3 c = ToFloat3(triangle.c);
    const Float3 boundsMin = Min(a, Min(b, c));
    const Float3 boundsMax = Max(a, Max(b, c));

    return {
        .triangle = triangle,
        .boundsMin = boundsMin,
        .boundsMax = boundsMax,
        .centroid = (boundsMin + boundsMax) * 0.5f,
    };
}

Bounds ComputeBounds(const std::vector<TriangleRef>& refs, std::size_t start, std::size_t end)
{
    Bounds bounds;
    for (std::size_t index = start; index < end; ++index)
    {
        bounds.min = Min(bounds.min, refs[index].boundsMin);
        bounds.max = Max(bounds.max, refs[index].boundsMax);
    }
    return bounds;
}

Bounds ComputeCentroidBounds(
    const std::vector<TriangleRef>& refs,
    std::size_t start,
    std::size_t end
)
{
    Bounds bounds;
    for (std::size_t index = start; index < end; ++index)
    {
        bounds.min = Min(bounds.min, refs[index].centroid);
        bounds.max = Max(bounds.max, refs[index].centroid);
    }
    return bounds;
}

std::uint32_t BuildNode(
    std::vector<TriangleRef>& refs,
    std::vector<GpuBvhNode>& nodes,
    std::size_t start,
    std::size_t end
)
{
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes.size());
    nodes.push_back({});

    const Bounds bounds = ComputeBounds(refs, start, end);
    const std::size_t count = end - start;
    if (count <= kLeafTriangleCount)
    {
        nodes[nodeIndex] = {
            .boundsMin = ToFloat4(bounds.min, 0.0f),
            .boundsMax = ToFloat4(bounds.max, 0.0f),
            .meta = {
                static_cast<std::uint32_t>(start),
                static_cast<std::uint32_t>(count),
                1u,
                0u,
            },
        };
        return nodeIndex;
    }

    const Bounds centroidBounds = ComputeCentroidBounds(refs, start, end);
    const Float3 centroidExtent = centroidBounds.max - centroidBounds.min;
    int axis = 0;
    if (centroidExtent.y > centroidExtent.x && centroidExtent.y >= centroidExtent.z)
    {
        axis = 1;
    }
    else if (centroidExtent.z > centroidExtent.x && centroidExtent.z >= centroidExtent.y)
    {
        axis = 2;
    }

    const std::size_t mid = start + count / 2;
    std::nth_element(
        refs.begin() + static_cast<std::ptrdiff_t>(start),
        refs.begin() + static_cast<std::ptrdiff_t>(mid),
        refs.begin() + static_cast<std::ptrdiff_t>(end),
        [axis](const TriangleRef& a, const TriangleRef& b)
        {
            if (axis == 0)
            {
                return a.centroid.x < b.centroid.x;
            }
            if (axis == 1)
            {
                return a.centroid.y < b.centroid.y;
            }
            return a.centroid.z < b.centroid.z;
        }
    );

    const std::uint32_t leftChild = BuildNode(refs, nodes, start, mid);
    const std::uint32_t rightChild = BuildNode(refs, nodes, mid, end);
    nodes[nodeIndex] = {
        .boundsMin = ToFloat4(bounds.min, 0.0f),
        .boundsMax = ToFloat4(bounds.max, 0.0f),
        .meta = {leftChild, rightChild, 0u, 0u},
    };
    return nodeIndex;
}
} // namespace

std::vector<GpuBvhNode> BuildTriangleBvh(std::vector<GpuTriangle>& triangles)
{
    if (triangles.empty())
    {
        return {};
    }

    std::vector<TriangleRef> refs;
    refs.reserve(triangles.size());
    for (const GpuTriangle& triangle : triangles)
    {
        refs.push_back(BuildRef(triangle));
    }

    std::vector<GpuBvhNode> nodes;
    nodes.reserve(triangles.size() * 2);
    BuildNode(refs, nodes, 0, refs.size());

    for (std::size_t index = 0; index < refs.size(); ++index)
    {
        triangles[index] = refs[index].triangle;
    }

    return nodes;
}
