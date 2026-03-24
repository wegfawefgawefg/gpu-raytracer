#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#pragma clang diagnostic pop

#include "mesh_loader.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
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

Float3 ReadPosition(const tinyobj::attrib_t& attrib, tinyobj::index_t index)
{
    const int offset = 3 * index.vertex_index;
    return {
        attrib.vertices[static_cast<std::size_t>(offset)],
        attrib.vertices[static_cast<std::size_t>(offset + 1)],
        attrib.vertices[static_cast<std::size_t>(offset + 2)],
    };
}

Float3 NormalizeVertex(
    const Float3& vertex,
    const Float3& center,
    float extent,
    const ObjMeshLoadParams& params
)
{
    Float3 local = vertex;
    if (params.normalize)
    {
        local = (local - center) / extent;
    }

    return {
        local.x * params.scale.x + params.position.x,
        local.y * params.scale.y + params.position.y,
        local.z * params.scale.z + params.position.z,
    };
}
} // namespace

std::vector<GpuTriangle> LoadObjTriangles(const ObjMeshLoadParams& params)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;

    const std::string path(params.path);
    const std::string basePath =
        std::filesystem::path(path).parent_path().string() + "/";
    const bool loaded = tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warning,
        &error,
        path.c_str(),
        basePath.c_str(),
        true
    );
    if (!loaded)
    {
        throw std::runtime_error(
            "Failed to load OBJ '" + path + "': " + error
        );
    }

    Float3 boundsMin = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    Float3 boundsMax = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };

    for (std::size_t index = 0; index + 2 < attrib.vertices.size(); index += 3)
    {
        const Float3 vertex = {
            attrib.vertices[index],
            attrib.vertices[index + 1],
            attrib.vertices[index + 2],
        };
        boundsMin = Min(boundsMin, vertex);
        boundsMax = Max(boundsMax, vertex);
    }

    const Float3 center = (boundsMin + boundsMax) * 0.5f;
    const Float3 extent3 = boundsMax - boundsMin;
    const float extent = std::max(std::max(extent3.x, extent3.y), std::max(extent3.z, 1e-6f));

    std::vector<GpuTriangle> triangles;
    for (const tinyobj::shape_t& shape : shapes)
    {
        const auto& mesh = shape.mesh;
        std::size_t indexOffset = 0;
        for (unsigned char faceVertexCount : mesh.num_face_vertices)
        {
            if (faceVertexCount != 3)
            {
                indexOffset += faceVertexCount;
                continue;
            }

            const Float3 a = NormalizeVertex(
                ReadPosition(attrib, mesh.indices[indexOffset]),
                center,
                extent,
                params
            );
            const Float3 b = NormalizeVertex(
                ReadPosition(attrib, mesh.indices[indexOffset + 1]),
                center,
                extent,
                params
            );
            const Float3 c = NormalizeVertex(
                ReadPosition(attrib, mesh.indices[indexOffset + 2]),
                center,
                extent,
                params
            );

            triangles.push_back({
                .a = ToFloat4(a, 0.0f),
                .b = ToFloat4(b, 0.0f),
                .c = ToFloat4(c, 0.0f),
                .albedoKind = {
                    params.albedo.x,
                    params.albedo.y,
                    params.albedo.z,
                    static_cast<float>(params.materialKind),
                },
                .emission = ToFloat4(params.emission, 0.0f),
            });

            indexOffset += 3;
        }
    }

    if (triangles.empty())
    {
        throw std::runtime_error("OBJ loaded but produced no triangles: " + path);
    }

    return triangles;
}
