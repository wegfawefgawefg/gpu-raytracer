#include "sample_scene.h"

#include "bvh_builder.h"
#include "mesh_loader.h"
#include "texture_loader.h"

namespace
{
GpuMaterial MakeMaterial(
    Float3 albedo,
    float roughness,
    Float3 emission,
    MaterialKind kind,
    std::uint32_t textureIndex
)
{
    return {
        .albedoRoughness = {albedo.x, albedo.y, albedo.z, roughness},
        .emission = {emission.x, emission.y, emission.z, 0.0f},
        .meta = {static_cast<std::uint32_t>(kind), textureIndex, 0u, 0u},
    };
}
} // namespace

SceneData BuildSampleScene(
    std::string_view meshPath,
    std::string_view texturePath
)
{
    SceneData scene;

    scene.materials = {
        MakeMaterial({0.75f, 0.78f, 0.82f}, 1.0f, {0.0f, 0.0f, 0.0f}, MaterialKind::Diffuse, kInvalidTextureIndex),
        MakeMaterial({0.94f, 0.96f, 0.99f}, 0.0f, {0.0f, 0.0f, 0.0f}, MaterialKind::Mirror, kInvalidTextureIndex),
        MakeMaterial({1.0f, 0.98f, 0.95f}, 0.0f, {18.0f, 17.0f, 15.0f}, MaterialKind::Emissive, kInvalidTextureIndex),
        MakeMaterial({1.0f, 1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 0.0f}, MaterialKind::Diffuse, 0u),
    };

    const auto defaultSpheres = BuildDefaultScene();
    scene.spheres.assign(defaultSpheres.begin(), defaultSpheres.end());

    LoadedTexture texture = LoadTextureRgba8(texturePath);
    scene.textures.push_back({
        .sizeOffset = {
            texture.width,
            texture.height,
            0u,
            0u,
        },
    });
    scene.texturePixels = std::move(texture.pixels);

    scene.triangles = LoadObjTriangles({
        .path = meshPath,
        .position = {0.0f, 0.10f, 0.0f},
        .scale = {2.5f, 2.5f, 2.5f},
        .rotationDegrees = {-90.0f, 0.0f, 0.0f},
        .normalize = true,
        .materialIndex = 3u,
    });
    scene.bvhNodes = BuildTriangleBvh(scene.triangles);

    return scene;
}
