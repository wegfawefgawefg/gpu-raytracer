# Current Renderer Walkthrough

This document explains how the renderer in `gpu-raytracer` works right now.

It is intentionally about the current code, not about an ideal future design.

## What It Does Today

The program opens an SDL3 window, creates a Vulkan swapchain, runs a compute shader that raytraces a tiny hardcoded sphere scene into an offscreen image, and then blits that image into the swapchain image for presentation.

The scene currently has:

- a giant ground sphere
- one diffuse red sphere
- one mirror sphere
- one emissive light sphere

The camera is controlled on the CPU side and its state is uploaded to the GPU every frame.

## High-Level Flow

Each frame looks like this:

1. SDL polls input and window events.
2. CPU updates camera state.
3. CPU writes camera/render parameters into a mapped Vulkan uniform buffer.
4. Vulkan acquires a swapchain image.
5. Vulkan dispatches the compute shader over the render-resolution image.
6. The compute shader writes one color per pixel into a storage image.
7. Vulkan blits that storage image into the swapchain image.
8. Vulkan presents the swapchain image to the window.

That is the whole renderer path at the moment.

## File Map

These are the important files:

- [`src/main.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/main.cpp): tiny entrypoint
- [`src/app.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/app.h)
- [`src/app.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/app.cpp): SDL window, input, frame loop
- [`src/camera.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/camera.h)
- [`src/camera.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/camera.cpp): camera position/orientation and movement
- [`src/gpu_types.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/gpu_types.h): structs shared conceptually between CPU scene setup and shader-side expectations
- [`src/math_types.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/math_types.h): tiny local vector math helpers
- [`src/vulkan_helpers.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/vulkan_helpers.h)
- [`src/vulkan_helpers.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/vulkan_helpers.cpp): file loading, Vulkan checks, buffer/image allocation
- [`src/vulkan_renderer.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/vulkan_renderer.h)
- [`src/vulkan_renderer.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/vulkan_renderer.cpp): Vulkan instance/device/swapchain/pipeline/render logic
- [`shaders/raytrace.comp`](/home/vega/Coding/Graphics/gpu-raytracer/shaders/raytrace.comp): compute shader raytracer

## App Layer

`App` is the top-level state holder.

Its main responsibilities are:

- initialize SDL and create the window
- keep the window floating on your X11/i3 setup
- handle keyboard and mouse input
- manage the camera
- manage resolution scaling
- call into `VulkanRenderer`

The main loop in [`src/app.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/app.cpp) is:

1. poll SDL events
2. update the camera from keyboard input
3. build a `GpuFrameParams` struct
4. hand that struct to `VulkanRenderer::Render`

That is deliberately simple.

## Camera

`Camera` stores:

- position
- yaw
- pitch
- vertical FOV

It computes three basis vectors:

- forward
- right
- up

Those basis vectors are not sent to the GPU as a matrix. Instead, the CPU sends:

- camera position
- camera forward
- camera right scaled by horizontal image-plane half-width
- camera up scaled by vertical image-plane half-height

That means the shader can build a ray direction for each pixel with:

- forward
- plus horizontal screen offset times right
- plus vertical screen offset times up

This is done in [`src/camera.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/camera.cpp) and consumed in [`shaders/raytrace.comp`](/home/vega/Coding/Graphics/gpu-raytracer/shaders/raytrace.comp).

## CPU Scene Data

The current scene is hardcoded in [`src/gpu_types.h`](/home/vega/Coding/Graphics/gpu-raytracer/src/gpu_types.h).

`GpuSphere` contains:

- `centerRadius`: xyz = center, w = radius
- `albedoKind`: xyz = albedo color, w = material kind
- `emission`: xyz = emitted light

Material kind is currently one of:

- diffuse
- emissive
- mirror

This is already a GPU-style layout:

- flat array
- fixed-size records
- no pointers
- no virtual dispatch

That is a major architectural difference from a typical CPU raytracer object graph.

## Frame Parameters

`GpuFrameParams` is uploaded every frame through a mapped uniform buffer.

It contains:

- camera position
- camera forward
- camera right
- camera up
- render info

`renderInfo` currently packs:

- render width
- render height
- one unused float slot
- sphere count

That is not the final long-term design. It is just the current compact parameter block.

## Vulkan Setup

The Vulkan work lives in [`src/vulkan_renderer.cpp`](/home/vega/Coding/Graphics/gpu-raytracer/src/vulkan_renderer.cpp).

### Initialization

Startup does this:

1. create Vulkan instance
2. create SDL Vulkan surface
3. pick a physical device
4. pick one queue family that supports graphics, compute, and present
5. create logical device
6. create command pool and command buffer
7. create semaphores and fence
8. create CPU-visible uniform/storage buffers
9. create descriptor set layout and descriptor pool
10. load the compiled compute shader SPIR-V
11. create compute pipeline

The shader is compiled at build time by CMake from [`shaders/raytrace.comp`](/home/vega/Coding/Graphics/gpu-raytracer/shaders/raytrace.comp) into `build/debug/shaders/raytrace.comp.spv`.

### Resize Path

When the window size or internal render scale changes:

1. destroy old swapchain/render target resources
2. create a new swapchain sized to the window
3. create a new offscreen render target sized to the chosen internal render resolution
4. update descriptors so the compute shader sees the new storage image

The swapchain size follows the window.

The render target size follows:

- `F1`: full window resolution
- `F2`: half resolution
- `F3`: quarter resolution

The renderer then blits the low-res render target to the full window size.

## Descriptor Bindings

The compute shader currently sees three bindings:

- binding `0`: storage image output
- binding `1`: uniform buffer with frame params
- binding `2`: storage buffer with spheres

That is the complete GPU resource model right now.

## What Happens In Render()

`VulkanRenderer::Render` does this:

1. copy `GpuFrameParams` into mapped uniform memory
2. wait for the previous frame fence
3. acquire the next swapchain image
4. reset and record the command buffer
5. submit the command buffer
6. present the swapchain image

There is only one in-flight frame right now. That keeps the code easier to read.

## Command Buffer Recording

The command buffer does three important things:

### 1. Transition layouts

The code moves:

- the storage image into a layout suitable for shader writes
- the swapchain image into a layout suitable for transfer destination writes

Vulkan requires explicit layout transitions. Nothing happens implicitly.

### 2. Dispatch compute

The command buffer binds:

- the compute pipeline
- the descriptor set

Then it dispatches enough workgroups to cover the render target.

The workgroup size is currently `16x16`.

So the CPU computes:

- `groupsX = ceil(renderWidth / 16)`
- `groupsY = ceil(renderHeight / 16)`

### 3. Blit into the swapchain image

After compute finishes writing the storage image:

- the storage image becomes a transfer source
- Vulkan blits it into the swapchain image
- the swapchain image becomes presentable

That blit is why resolution scaling is easy right now. The compute shader can render a smaller image and Vulkan can scale it to the window.

## Shader Structure

The compute shader is in [`shaders/raytrace.comp`](/home/vega/Coding/Graphics/gpu-raytracer/shaders/raytrace.comp).

Each invocation owns one pixel.

The shader does:

1. compute pixel coordinates from `gl_GlobalInvocationID`
2. reject out-of-range invocations
3. map pixel to normalized screen coordinates
4. build the primary camera ray
5. run a small bounce loop
6. write final color to the storage image

## Ray Generation

For each pixel:

- `uv` is computed in `[0, 1]`
- `screen` is remapped to `[-1, 1]`

The ray direction is:

`normalize(cameraForward + screen.x * cameraRight + screen.y * cameraUp)`

This is the GPU version of camera ray generation.

## Scene Intersection

`traceScene()` just loops over all spheres and keeps the nearest hit.

That means:

- no BVH yet
- no mesh support yet
- O(n) intersection over the sphere list

This is fine for the current teaching-stage renderer.

The important thing is that even now it is written in GPU style:

- iterate over a packed array
- no CPU-side polymorphism
- no dynamic dispatch

## Lighting

Diffuse lighting currently uses a very simple direct-light model.

For diffuse surfaces:

1. pick the emissive sphere as the light source
2. compute direction and distance to it
3. cast a shadow ray
4. if not shadowed, add Lambert-like direct lighting with inverse-square attenuation
5. add a small ambient term

This is not yet a Monte Carlo path tracer.

It is a direct-light teaching step that gets something readable on screen while the compute architecture stays simple.

## Reflections

Mirror materials are where the current code starts to look like a real GPU raytracer.

The shader maintains:

- `rayOrigin`
- `rayDirection`
- `throughput`

Inside a bounce loop.

For each bounce:

1. intersect the scene
2. if miss, add sky times throughput and stop
3. add surface emission times throughput
4. if diffuse, add direct lighting and stop
5. if mirror, reflect the ray and continue

This is the beginning of the standard GPU path-tracing structure:

- iterative loop
- explicit ray state
- explicit throughput
- explicit material branching

That is the main architectural lesson in the current shader.

## Why This Is GPU-Style Already

Even though the renderer is still simple, it already differs from a CPU-style renderer in the important ways:

- scene data is packed into flat buffers
- material behavior is selected by integer tag
- ray tracing is iterative, not recursive
- GPU threads write into an image directly
- CPU prepares data and dispatches work, but does not trace rays itself

That is why this is a good base to learn from.

## What Is Still Simplified

Several things are intentionally minimal right now:

- hardcoded scene
- only spheres
- one hardcoded light
- one command buffer
- one frame in flight
- no BVH
- no accumulation
- no random sampling
- no diffuse bounce continuation
- no material roughness / refraction
- no mesh data

So this is not yet a full GPU path tracer.

It is a clean first compute-raytracing skeleton.

## Good Questions To Ask While Reading

As you inspect the code, useful questions are:

- Why is the camera basis precomputed on the CPU instead of in the shader?
- Why is the scene buffer flat instead of object-oriented?
- Why does the mirror path continue but the diffuse path stop?
- Why is the swapchain only a presentation target and not written directly by the compute shader?
- What changes when we add accumulation?
- What changes when we add triangles?
- What changes when we replace the O(n) sphere loop with a BVH?

Those are the next real architectural jumps from the current code.

## What I Would Teach Next

If the goal is to keep building this in learning order, the next topics should be:

1. progressive accumulation
2. random number state per pixel
3. diffuse bounce sampling
4. separating direct light from indirect bounce logic
5. triangles and mesh buffers
6. BVH traversal

That sequence keeps the renderer understandable while steadily moving it toward a real GPU path tracer.
