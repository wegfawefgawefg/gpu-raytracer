# Bake Volume Notes

This document describes a practical editor-style baking workflow for static lighting.

The goal is not just "how lightmaps work," but how a real tool might expose baking in a way that is usable for a game.

## The Basic Idea

Instead of rebaking an entire game world every time, the engine can let you define a bake region.

For example:

1. place a 3D AABB or volume in the world
2. mark it as a bake region
3. press a `Bake` button
4. show a progress bar while rays are traced
5. save baked lighting for content inside that region

Then that region can use prebaked static lighting at runtime.

This is a practical workflow because it scopes the expensive work.

## Why A Bake Volume Is Useful

Baking the entire world all the time is often wasteful.

A bake volume lets the engine focus on:

- important interior spaces
- authored gameplay areas
- static geometry clusters
- places where indirect lighting matters visually

This avoids spending the same effort on:

- huge empty terrain
- distant or low-value regions
- temporary content
- areas still changing rapidly during iteration

## What Gets Baked

A bake volume does not necessarily mean only one output type.

A region bake might generate:

- lightmaps for static surfaces
- probes for dynamic objects inside the space
- reflection captures
- cached irradiance fields

The exact output depends on the renderer.

For a simple static-lighting pipeline, the first useful result is usually:

- baked lightmaps for static geometry in the volume

## How The Workflow Might Look

An editor-oriented flow could be:

1. user places a bake volume
2. engine gathers static geometry and static lights intersecting or affecting that volume
3. engine builds the bake job
4. baker traces many rays for surfaces and/or probes in the region
5. progress UI shows:
   - current asset / chunk
   - percent complete
   - elapsed time
   - maybe sample count or bounce count
6. baked data is written to disk
7. runtime renderer uses that baked data

This is much more practical than pretending all baking must be a single global action.

## Why Large Volumes Are Hard

Your intuition is right: very large open regions are usually a bad fit for dense baking.

Big bake volumes can cause:

- long bake times
- more memory use
- lower effective sample density
- noisier results if sample count is fixed
- lots of wasted work on empty space

So a bake volume is most useful when it is:

- sized to the authored space
- not much larger than the important static lighting area
- broken into manageable chunks when necessary

## Quality Controls A Bake Tool Would Want

A usable tool would probably expose knobs like:

- lightmap resolution
- samples per texel
- max bounce count
- probe density
- denoise on/off
- bake direct only vs direct + indirect

That gives artists or developers a tradeoff between:

- bake speed
- memory
- final quality

## Static Vs Dynamic Split

A bake volume mostly makes sense for static content.

Typical rules would be:

- static geometry: included in bake
- static lights: included in bake
- dynamic objects: usually not baked directly
- dynamic lights: usually runtime only or partially baked depending on engine

That means the baked result often becomes:

- a stable baseline for the static world

and runtime lighting then adds:

- moving lights
- moving objects
- dynamic shadows

## Relationship To The Current Repo

If this repo ever experiments with baking, a bake-volume workflow would be a good eventual direction.

The likely sequence would be:

1. first prove one-mesh lightmap baking
2. then add region-scoped bake jobs
3. then add editor-style volume selection and progress reporting

So a bake volume is not the first thing to build, but it is a very reasonable design target for a practical static-lighting tool.

## Bottom Line

Yes, a bake-volume workflow is a good idea.

It matches how a usable game pipeline would likely avoid global rebakes, and it fits naturally with:

- progress bars
- partial rebakes
- region-based static-lighting authoring
- balancing quality against time and memory
