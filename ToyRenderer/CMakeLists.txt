find_package(Vulkan REQUIRED)

add_executable(ToyRenderer
    src/main.cpp
    src/app.h
    src/app.cpp
    src/options.h
    src/options.cpp
    src/renderer/device.h
    src/renderer/device.cpp
    src/renderer/swapchain.h
    src/renderer/swapchain.cpp
    src/renderer/renderpass.h
    src/renderer/renderpass.cpp
    src/renderer/command_pool.h
    src/renderer/command_pool.cpp
    src/renderer/instance.h
    src/renderer/instance.cpp
    src/renderer/surface.h
    src/renderer/surface.cpp
    src/renderer/vulkan_engine.h
    src/renderer/vulkan_engine.cpp
    src/renderer/deletion_queue.h
    src/renderer/deletion_queue.cpp
    src/renderer/utils.h
    src/renderer/utils.cpp
    src/renderer/debug.h
    src/renderer/debug.cpp
    src/renderer/extensions.cpp
    src/renderer/extensions.h
    src/renderer/pipeline.cpp
    src/renderer/pipeline.h
    src/renderer/vertex.cpp
    src/renderer/vertex.h
    src/system/platform.h
    src/system/platform.cpp
    src/system/input.h
    src/system/input.cpp
    src/system/imgui.h
)
target_link_libraries(ToyRenderer PRIVATE glfw Vulkan::Vulkan imgui_glfw_vulkan spdlog glm utils renderdoc)

# set_property(TARGET ToyRenderer PROPERTY CXX_CLANG_TIDY "clang-tidy")
# set_property(TARGET ToyRenderer PROPERTY CXX_INCLUDE_WHAT_YOU_USE "iwyu-tool")

if (MSVC)
    target_compile_options(ToyRenderer PRIVATE /W4)
else ()
    target_compile_options(ToyRenderer PRIVATE -Wall -Wextra -fdiagnostics-color=always -ggdb )
endif ()
