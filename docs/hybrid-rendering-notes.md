# Hybrid Rendering Notes

This note captures the main hybrid-rendering ideas discussed while building `gpu-raytracer`.

The current project is a compute raytracer. Modern games usually do something different.

## Full Path Tracing Vs Hybrid Rendering

The current renderer does:

- camera ray generation on the GPU
- scene intersection in the raytracing shader
- shading and bounce logic in the same general raytracing path
- optional accumulation over many frames

A hybrid game renderer usually does:

- rasterization for primary visibility
- a G-buffer pass for visible-surface data
- selective ray tracing for secondary effects
- denoising and temporal reuse on those secondary effects

So hybrid rendering is usually not "path trace the whole final image every frame."

It is more like:

- rasterize the scene first
- then ask a few targeted ray-traced questions

## What A G-Buffer Is

A G-buffer is a set of per-pixel surface records produced by rasterization.

Typical contents:

- depth
- normal
- albedo or base color
- roughness / metalness
- material ID
- motion vectors

This means the renderer already knows the first visible surface at each screen pixel.

That is the main reason hybrid rendering is cheaper than full path tracing:

- full path tracer: solve primary visibility with camera rays
- hybrid renderer: rasterizer already solved primary visibility

## How Ray Tracing Fits Into A Hybrid Renderer

After rasterization, the engine can fire rays from the visible surface point instead of from the camera.

Examples:

- shadow ray: cast from the visible point toward a light
- reflection ray: cast from the visible point in the mirror direction
- GI / AO ray: cast from the visible point into the hemisphere around the normal

So "ray-traced shadows" or "ray-traced reflections" do not mean the whole frame was ray traced.

They mean:

- rasterization found the visible point
- ray tracing answered a secondary visibility or lighting question from that point

## Why Games Often Use Lower Resolution Ray Tracing

Real-time ray tracing is expensive, so engines often:

- trace at half or quarter resolution
- trace only a subset of effects
- trace only for some materials
- reuse previous frames

Then they reconstruct the final effect with:

- depth
- normals
- motion vectors
- temporal accumulation
- denoising

This is why ray-traced effects in games are often noisy under the hood even when the final frame looks stable.

## Why Players Notice Blur, Flicker, Or Smear

Hybrid rendering often trades image sharpness for stability and performance.

That comes from:

- temporal accumulation
- reprojection
- denoising
- lower-resolution effect buffers

That tradeoff is one reason some players dislike modern real-time RT settings even when the lighting looks more correct.

## Why This Repo Does Not Do That

`gpu-raytracer` was built as an educational compute raytracer first.

So it intentionally focuses on:

- explicit GPU scene packing
- BVH traversal
- shader-side ray generation and shading
- classic vs accumulated path tracing

instead of building:

- a raster pipeline
- a G-buffer
- motion vectors
- temporal reprojection
- denoising

Those would be natural next topics, but they are different lessons.

## If This Repo Ever Continued In The Hybrid Direction

The first reasonable steps would be:

1. add a raster mesh pass
2. write a small G-buffer
3. trace one selective effect, probably reflections or shadows
4. composite that effect back into the rasterized frame

That would make the repo more like a modern game renderer and less like a small standalone path tracer.
