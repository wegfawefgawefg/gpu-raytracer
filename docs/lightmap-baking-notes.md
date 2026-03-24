# Lightmap Baking Notes

This document explains how the current `gpu-raytracer` could be used to bake lighting into textures and then hand that result off to a raster renderer.

This is not implemented yet.

The point of this note is to answer:

- how close the current codebase is
- what pieces already exist
- what is still missing

## The High-Level Idea

Instead of tracing camera rays every frame, a baker traces rays from sampled surface points and stores the result into a texture.

Later, runtime rendering can be mostly rasterized:

1. rasterize the mesh normally
2. sample the baked lightmap texture in the material
3. get stable lighting cheaply at runtime

This is the classic static-lighting workflow.

## What We Already Have

The current repo already has several important prerequisites:

- OBJ mesh loading
- UV loading for triangles
- packed material data
- texture loading
- triangle BVH
- ray/triangle intersection
- diffuse bounce sampling
- emissive lighting

That means the hard part of light transport is no longer missing.

The codebase already knows how to:

- load a mesh
- map UVs onto triangles
- trace rays through that mesh
- compute direct and indirect diffuse light

That is a big part of a baker.

## What A Baker Would Do Differently

The current renderer traces from the camera.

A lightmap baker would instead:

1. choose a bake texture resolution
2. for each lightmap texel, find the corresponding point on a surface
3. build a ray origin and shading normal there
4. evaluate lighting at that point by tracing rays into the scene
5. write the result into the bake texture

So the main difference is the sampling domain:

- current renderer: per camera pixel
- baker: per lightmap texel

## Why UVs Matter

For baking, UV quality matters a lot.

Ordinary texture UVs are not always good lightmap UVs.

Good lightmap UVs should usually be:

- non-overlapping
- padded between islands
- stable under filtering
- laid out for light storage, not just color texture reuse

If you bake lighting into reused overlapping texture UVs, you often get:

- lighting leaks
- mirrored lighting where it should not mirror
- seams
- inconsistent texel density

So a hacky first bake can use existing UVs, but a proper bake workflow usually wants a dedicated lightmap UV set.

## The Minimum Hacky Version

The smallest educational version in this repo would be:

1. pick one static mesh
2. use its existing UVs as if they were lightmap UVs
3. choose a lightmap size, for example `512x512`
4. for each texel:
   - find a triangle and barycentric point that covers that texel
   - reconstruct world position and normal
   - trace direct and indirect lighting from that point
5. write the result into an RGBA image
6. save the baked texture
7. sample that texture later in a raster or hybrid renderer

This would be good enough to prove the workflow.

It would not yet be asset-pipeline quality.

## The Missing Pieces

To do this for real, the repo would need new pieces.

### 1. Surface-To-Texel Mapping

We need a bake pass that answers:

- which texel belongs to which triangle
- what barycentric coordinates correspond to that texel

This is not the same as camera visibility.

It is more like:

- iterate triangle UV coverage
- rasterize into lightmap space
- store triangle ID and barycentrics per texel

That could be done:

- on CPU
- in a GPU bake pass
- or with a temporary raster pass into a bake atlas

### 2. Output Texture Generation

We need a place to store the baked light values:

- CPU image buffer
- or GPU storage image/buffer

Then later write:

- PNG
- HDR image
- or some internal binary format

### 3. Raster Runtime Path

If the goal is "bake once, render fast later," then we also need a raster pipeline that can:

- draw the mesh
- sample its base texture
- sample its baked lightmap
- combine them in a fragment shader

That part is not in the repo yet.

### 4. Static/Dynamic Rules

A baked workflow usually distinguishes:

- static geometry and lights: baked
- dynamic objects/lights: runtime only

This repo does not yet have that split.

## How Good Would It Look

If the bake includes diffuse indirect rays, it can capture:

- soft static shadows
- bounced light
- color bleeding
- stable indirect lighting

That is one of the biggest advantages of baked lighting:

- high stability
- low runtime cost
- no temporal flicker

The tradeoff is:

- static scene assumptions
- bake times
- asset UV requirements

## Relationship To Probes

Lightmaps and probes solve different problems.

Lightmaps:

- store lighting on surfaces
- high detail
- good for static geometry

Probes:

- store lighting in world space
- lower detail
- useful for dynamic objects and ambient spatial lighting

A classic renderer often uses both:

- bake lightmaps for static world geometry
- sample probes for moving characters or props

## A Practical Next Experiment

If this repo ever explores baking, the clean first experiment would be:

1. keep the current BVH/material/texture path
2. add one `lightmap_baker.cpp`
3. bake a single static mesh into one texture
4. write the image to disk
5. do not worry about full runtime rasterization yet

That would prove the concept without turning the repo into a full engine.

## Bottom Line

The current project is already much closer to a lightmap baker than it was when it only traced camera rays against spheres.

What is missing is mostly:

- lightmap-space sampling
- bake output handling
- a raster runtime path if we want to display baked results that way

So:

- hacky educational bake: fairly close
- production lightmap pipeline: still a sizable amount of work
