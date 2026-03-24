# GPU Raytracer Roadmap

This is the current implementation order for `gpu-raytracer`.

The point is to learn the GPU architecture in a sequence that stays clear.

## Current State

We already have:

- Vulkan compute path tracing into a storage image
- accumulation over frames
- diffuse and mirror materials
- sphere geometry
- native-resolution UI composited over the traced image

That is enough to understand the core GPU loop.

## Next Milestones

### 1. Triangles And Mesh Loading

First real step after spheres:

- add a flat triangle buffer
- load one OBJ mesh on the CPU
- upload triangles to the GPU
- intersect triangles in the compute shader

This is the first useful mesh milestone even if it is brute force and slow.

The goal is not optimization yet. The goal is to prove:

- CPU asset loading
- GPU-friendly scene packing
- triangle intersection
- a real model on screen

### 2. BVH

Once meshes are working, brute-force triangle tracing stops scaling.

Then add:

- flat BVH node buffer
- iterative traversal in shader
- mesh scenes that do not collapse under triangle count

This is the point where the renderer becomes structurally serious.

### 3. Better Material Data

After geometry and acceleration are stable, improve the material model:

- tagged material records
- roughness / metalness style fields
- emissive materials
- cleaner material IDs instead of geometry-owned ad hoc color fields

Do not jump to a giant material system before the geometry path is settled.

### 4. Textures

After material records exist, add:

- texture image loading
- texture uploads
- UV sampling in shader
- texture indices in material records

This is where textured meshes start making sense.

### 5. More Lighting Features

Then start pushing lighting transport quality:

- better direct light sampling
- environment lighting
- refractive materials
- eventually caustic-oriented work if we want it

## About Bounce Limits

The current shader has a fixed inner-loop bounce count.

Accumulation helps because many noisy finite-bounce samples converge over time, but it does not
remove the bounce limit by itself.

If we later want a less megakernel-style architecture, the real next idea is:

- persistent ray/path state buffers
- next-bounce queues
- multiple passes that continue surviving paths

That is wavefront-style path tracing.

So yes, there is a real architecture where rays survive across passes and are resumed later. That
is a future optimization / architecture step, not the next milestone.

## Related Notes

- `current-renderer-walkthrough.md`: what the renderer does right now
- `lightmap-baking-notes.md`: how this codebase could bake static lighting into textures
- `bake-volume-notes.md`: how a practical editor-facing bake-volume workflow could look

## Immediate Target

The immediate target is:

1. load one OBJ mesh
2. render it with brute-force triangles
3. keep the current sphere light / simple material path
4. only then add BVH

That keeps the learning path clean.
