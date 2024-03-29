﻿find_package(fastgltf REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(unofficial-shaderc REQUIRED)
find_package(spdlog REQUIRED)
find_package(Stb REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

add_library(Shaders INTERFACE)
target_compile_shader(Shaders
    ENV vulkan1.3
    EXT inc
    FORMAT num
    ARGS -g
    SOURCES
    shaders/debug.frag
    shaders/debug.vert
    shaders/deferred.frag
    shaders/deferred.vert
    shaders/forward.frag
    shaders/forward.vert
    shaders/gbuffer.frag
    shaders/gbuffer.vert
    shaders/present.frag
    shaders/present.vert
    shaders/shadow_map.vert
    shaders/ssao.frag
    shaders/ssao.vert
)

target_include_directories(Shaders INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)

add_executable(ToyRenderer
    src/app.cpp
    src/app.h
    src/camera.cpp
    src/camera.h
    src/gltf.cpp
    src/gltf.h
    src/main.cpp
    src/registry.cpp
    src/registry.h
    src/options.cpp
    src/options.h
    src/renderer/buffer.h
    src/renderer/command_pool.cpp
    src/renderer/command_pool.h
    src/renderer/constants.h
    src/renderer/context.cpp
    src/renderer/context.h
    src/renderer/debug.cpp
    src/renderer/debug.h
    src/renderer/deletion_stack.cpp
    src/renderer/deletion_stack.h
    src/renderer/descriptors.cpp
    src/renderer/descriptors.h
    src/renderer/device.cpp
    src/renderer/device.h
    src/renderer/extensions.cpp
    src/renderer/extensions.h
    src/renderer/frame.cpp
    src/renderer/frame.h
    src/renderer/instance.cpp
    src/renderer/instance.h
    src/renderer/mesh.h
    src/renderer/passes/debug.cpp
    src/renderer/passes/debug.h
    src/renderer/passes/deferred.cpp
    src/renderer/passes/deferred.h
    src/renderer/passes/forward.cpp
    src/renderer/passes/forward.h
    src/renderer/passes/frustrum_culling.cpp
    src/renderer/passes/frustrum_culling.h
    src/renderer/passes/gbuffer.cpp
    src/renderer/passes/gbuffer.h
    src/renderer/passes/pass.cpp
    src/renderer/passes/pass.h
    src/renderer/passes/present.cpp
    src/renderer/passes/present.h
    src/renderer/passes/shadow_map.cpp
    src/renderer/passes/shadow_map.h
    src/renderer/passes/ssao.cpp
    src/renderer/passes/ssao.h
    src/renderer/pipeline.cpp
    src/renderer/pipeline.h
    src/renderer/queue.h
    src/renderer/render_graph.cpp
    src/renderer/render_graph.h
    src/renderer/ressource_definition.h
    src/renderer/ressource_manager.cpp
    src/renderer/ressource_manager.h
    src/renderer/ressources.cpp
    src/renderer/ressources.h
    src/renderer/surface.cpp
    src/renderer/surface.h
    src/renderer/swapchain.cpp
    src/renderer/swapchain.h
    src/renderer/synchronisation.h
    src/renderer/uploader.cpp
    src/renderer/uploader.h
    src/renderer/utils.cpp
    src/renderer/utils.h
    src/renderer/vkformat.h
    src/renderer/vma.cpp
    src/renderer/vulkan_engine.cpp
    src/renderer/vulkan_engine.h
    src/system/imgui.cpp
    src/system/imgui.h
    src/system/input.cpp
    src/system/input.h
    src/system/platform.cpp
    src/system/platform.h
)

target_compile_definitions(ToyRenderer PRIVATE -DGLM_ENABLE_EXPERIMENTAL)
target_link_libraries(ToyRenderer PRIVATE Shaders glfw Vulkan::Vulkan imgui::imgui spdlog::spdlog glm::glm utils renderdoc JsonCpp::JsonCpp GPUOpen::VulkanMemoryAllocator fastgltf::fastgltf unofficial::shaderc::shaderc)
target_include_directories(ToyRenderer PRIVATE ${Stb_INCLUDE_DIR})

# set_property(TARGET ToyRenderer PROPERTY CXX_CLANG_TIDY "clang-tidy" "--fix")
# set_property(TARGET ToyRenderer PROPERTY CXX_INCLUDE_WHAT_YOU_USE "include-what-you-use" -Xiwyu --mapping_file ${CMAKE_SOURCE_DIR}/iwyu-mapping.yaml )

if(MSVC)
    target_compile_options(ToyRenderer PRIVATE /W4)
else()
    target_compile_options(ToyRenderer PRIVATE -Wall -Wextra -fdiagnostics-color=always -ggdb -Werror=conversion -Werror=shadow ) # -Wpadded)
    # target_compile_options(ToyRenderer PRIVATE -fsanitize=address)
    # target_link_options(ToyRenderer PRIVATE -fsanitize=address)
    # target_compile_options(ToyRenderer PRIVATE -ftime-trace)
endif()
