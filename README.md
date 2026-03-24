# gpu-raytracer

GPU-native raytracer playground. `raytrace-rs` is the CPU-side reference for scene ideas and rendering intent, but this repo should stay compute/GPU-oriented rather than trying to mirror the CPU architecture directly.

## Current State

- CMake + Ninja build
- Clang toolchain
- Vendored SDL3 through CMake `FetchContent`
- Vendored SDL3_ttf for native-resolution text overlay
- SDL3 floating window on X11/i3
- Vulkan swapchain + compute shader raytracing path
- Progressive accumulation path tracer with movable camera
- Sphere scene plus first OBJ mesh path through a flat triangle buffer
- VS Code `F5` build + launch flow

## Build

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/gpu-raytracer
```

## VS Code

Open the `gpu-raytracer` folder and press `F5`.

That runs the `build` task, then launches `build/debug/gpu-raytracer` under `gdb`.

## Controls

- `Right Mouse`: hold to capture mouse and look around
- `W A S D`: move
- `Space` / `Shift`: move up / down
- `F1`: full internal resolution
- `F2`: half internal resolution
- `F3`: quarter internal resolution
- `Escape`: quit

## Next Steps

- Add a GPU-friendly BVH for the new triangle path
- Split geometry/material data more cleanly now that meshes exist
- Add texture uploads and textured material sampling
