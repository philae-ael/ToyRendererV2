include(FetchContent)

FetchContent_Declare(
        vulkan
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_SHALLOW ON
        GIT_TAG main
)

FetchContent_GetProperties(vulkan)
if (NOT vulkan_POPULATED)
    FetchContent_Populate(vulkan)

    add_subdirectory(${vulkan_SOURCE_DIR} ${vulkan_BINARY_DIR})
endif ()
