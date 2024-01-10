# YetAnotherRenderSandbox

This codebase serves as an on-going hobby project to familiarise myself with some modern C++ paradigms as well as some low-level Vulkan concepts.

The project consists of a Windows-specific solution file with dependencies pre-compiled with AVX2 as a pre-requisite which may render some hardware configurations unable to compile this codebase.

The current focus is on building up a fairly solid foundation, with graphical fidelity not being the immediate goal which will result in some sub-par output. Currently a hard-coded directional light spins around an arbitrary GLTF file (with the assumption it contains PBR data.)

## Current features:
* Render graph system that manages render dependencies between render passes as well as sharing buffer and texture resources.
* Integration with [ImGui}(https://github.com/ocornut/imgui) UI system.
* Asset import pipeline to convert GLTF data to BC5 & BC7 textures & optimised mesh data.
* Temporal anti-aliasing with minimal ghosting.
* Asset cache system to store imported assets in ready-to-consume GPU buffer data, currently LZ4 compressed on disk.
* HDR display output support.
* GPU-based frustum culling.
* (mostly working) Hi-Z GPU-based occlusion culling.

## Screenshots
### Options & Rendering
![preview](ReadmeAssets/preview_v4.png)

### Frame statistics
![stats](ReadmeAssets/stats.png)

### Render graph
![render_graph](ReadmeAssets/render_graph.png)

## Some short-term goals I'll be looking at:
* Finishing up the compute-based culling
	* Current depth checking in occlusion culling shader isn't really correct.
	* Also apply frustum culling to shadow rendering pass.
* Use drawIndirectCount instead of just drawIndirect.
	* Provides opportunity to clean up render graph by using 'generic buffer' entries for node graph.
* Add additional post-process AA variants (FXAA, SMAA & eventually DLAA).
* Considering adding some upscalers (DLSS, FSR & XeSS).
* Give synchronisation and render graph a once-over.
	* Currently there's no syncronisation validation errors, but would like to avoid creating messy, error-prone architecture.
	* Some simple sanity checks that applies even without Vulkan validation would be good.
	* Investigate if there's a nicer way to pass through "last frame" textures and buffers through the render graph (used for TAA and Hi-Z occlusion culling)

## Wishlist:
* Experimenting with basic animation.
* Get a basic transparency pass in.
	* Probably need a different test scene to display too.
* HDR improvements
	* Add some form of calibration - there's no 'nits' input right now.
	* Render UI to a regular SDR sRGB texture - currently it looks too bright and wrong.
* Handle asset streaming - currently everything is loaded into a few big buffers with no cheap way to load new data in.
* Possibly investigate replacing LZ4 compression with 'DirectStorage-like' GPU decompression during loading
* Look at raytracing!
* Use a proper build system (maybe CMake?) so that third-party dependencies aren't pre-compiled, which requires matching compiler versions to link.
	* Probably add some sort of shader compilation build step in there as well to avoid having to manually run a PowerShell script to compile these when shaders are updated.
	* Clean up the AVX2 requirements - make it a build option maybe, but don't hard-code it into the project.