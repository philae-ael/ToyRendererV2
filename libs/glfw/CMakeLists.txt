include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
)

FetchContent_GetProperties(glfw)
if (NOT glfw_POPULATED)
    FetchContent_Populate(glfw)

    add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
endif ()
