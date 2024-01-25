# Toy Renderer (V2)

## Build and Run

```
cmake -Bbuild -GNinja -DCMAKE_TOOLCHAIN_FILE=$(VCPKG)/scripts/buildsystems/vcpkg.cmake
./build/ToyRenderer/ToyRenderer
```

## Plan 

### Rendering
- [ ] GPU-Driven
- [ ] Mesh shaders
- [ ] Occlusion CPU & GPU
- [X] Frustrum culling
    - [X] AABB bounding box 
        - [ ] works but still buggy at the extremities of Sponza for big objects, the 8 rather than only 2 extremities should be tested, or something else idk
        - [ ] Rather than radar approch use hiearchical test? -> GPU maybe?
    - [ ] Maybe some more clever techniques like OOB / Convex Hull
- [ ] Allow to display wireframe / Bounding Box conditionnally
- [ ] Deferred Pipeline 
    - [X] PBR (buggy)
    - [ ] IBL
- [ ]  Forward  (for transparent objects)
- [ ] Bindless textures
- [ ] Ambient Occlusion
    - [ ] SSAO
- [ ] Shadows
    - [X] Basic Directionnal lights
    - [ ] Point lights
    - [ ] Shadow Cascade
    - [ ] Shadow Antialising
- [ ] Antialising
    - [ ] FXAA
    - [ ] TAA 
- [ ] More
    - [ ] Fog!
    - [ ] (basic) clothes simulation!
    - [ ] Fire

## [Libs](./vcpkg.json)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [FastGLTF](https://github.com/spnda/fastgltf)
- [GLFW](https://github.com/glfw/glfw)
- [GLM](https://github.com/g-truc/glm)
- [JsonCpp]() for reading / writing (for storing CVARS mainly)
- [Stbimage](https://github.com/nothings/stb)
- [Shaderc](https://github.com/google/shaderc) to recompile shaders during execution
- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [Vulkan](www.vulkan.org)

## Tools 
- [RenderDoc](https://renderdoc.org/)
- [Nvidia Nsight](https://developer.nvidia.com/nsight-graphics)

## References
### Rendering
- [RealTime Rendering](https://www.realtimerendering.com/)
- [Physically Based Rendering](https://pbrt.org/)

### Vulkan
- [Vulkan Guide](https://vkguide.dev/)
- [Vulkan Spec/Doc/...](www.vulkan.org)

## Math & Algorithms
- [Computational Geometry: Algorithms and Applications](https://link.springer.com/book/10.1007/978-3-540-77974-2)
- [Géométrie 1 et 2 de Marcel Berger (french)](https://www.amazon.fr/G%C3%A9om%C3%A9trie-1-Marcel-Berger/dp/2842251458)

## Other 
- Internet is a great ressource!
