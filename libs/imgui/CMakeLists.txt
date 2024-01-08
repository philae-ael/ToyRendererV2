
set(IMGUI_HEADERS
    imgui/imgui.h
    imgui/imgui_internal.h
    imgui/imstb_textedit.h
    imgui/imstb_rectpack.h
    imgui/imstb_truetype.h
    imgui/backends/imgui_impl_glfw.h
    imgui/backends/imgui_impl_vulkan.h
    imgui/misc/cpp/imgui_stdlib.h
    imgui/imconfig.h
)
add_library(imgui_glfw_vulkan STATIC
    imgui/imgui.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_HEADERS}
)

target_link_libraries(imgui_glfw_vulkan PRIVATE glfw vulkan)
file(COPY ${IMGUI_HEADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(imgui_glfw_vulkan PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
