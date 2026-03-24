# Packing Notes

This note is about where tighter data packing is likely to matter in this renderer, and where it usually does not.

It is not a rule that "smaller is always better." Packing is a bandwidth/performance tradeoff, not a free win.

## Low-Priority Packing Targets

These are usually not worth squeezing early:

- camera/frame parameter blocks
- a handful of light records
- a few debug or teaching-scene spheres
- tiny per-frame config structs

Why:

- total byte count is small
- update frequency is low
- they are not the dominant memory traffic in the renderer
- Vulkan/GLSL layout alignment often means you do not save much anyway

This is why `GpuFrameParams` and the current `GpuSphere` layout are fine as boring `Float4`-based structs.

## High-Priority Packing Targets

These become worth attention once the renderer grows:

### 1. Triangle / Mesh Data

This grows quickly with scene size.

Potential pressure points:

- positions
- normals
- UVs
- indices
- material IDs

Why it matters:

- lots of records
- read frequently
- big impact on memory bandwidth

### 2. BVH Nodes

A BVH can contain a large number of nodes, and each traversal touches many of them.

Why it matters:

- many nodes
- frequent random-ish access
- node size directly affects bandwidth and cache behavior

### 3. Per-Ray State

This gets important once the renderer carries many rays across multiple bounces.

Typical fields:

- origin
- direction
- throughput
- pixel index
- bounce count
- RNG state
- flags

Why it matters:

- potentially huge active ray counts
- every extra byte multiplies across all in-flight rays
- bloated ray state can hurt occupancy and bandwidth

### 4. Large Per-Pixel Buffers

Examples:

- accumulation buffers
- normals
- albedo
- depth
- variance
- denoising inputs

Why it matters:

- full-screen or larger
- scales with resolution
- often read and written every frame

### 5. Texture Data

This is often one of the biggest memory consumers in real renderers.

Why it matters:

- large total size
- heavy sampling traffic
- GPUs have many specialized compressed texture formats for this reason

## Why Smaller Types Are Not Automatically Better

Packing data more tightly can help:

- reduce memory usage
- reduce bandwidth
- improve cache behavior

But it can also cost:

- extra unpack/convert instructions
- more complicated shader logic
- alignment headaches
- lower portability
- worse access patterns if the packed layout becomes awkward

So packing only pays when the memory/bandwidth savings beat the extra compute and complexity.

## Common Practical Rule

Use boring, explicit layouts first.

Only start tighter packing when all three are true:

1. the data set is large
2. the data is hot
3. you have evidence the memory traffic matters

That is why the current renderer is fine with simple `float32`-based scene/config structs.

## Good Default Strategy

For this repo, a sensible order is:

1. keep small config structs simple
2. keep shader/CPU layouts obvious
3. optimize packing later in:
   - triangle buffers
   - BVH nodes
   - per-ray state
   - large per-pixel buffers

That gets most of the value without prematurely turning the renderer into a layout-debugging exercise.
