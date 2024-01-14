cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# For profiling
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
# SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

project("ToyRenderer")

find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

function(target_compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "EXT;ENV;FORMAT;ARGS" "SOURCES")
    foreach(source ${arg_SOURCES})
        add_custom_command(
            OUTPUT include/${source}.${arg_EXT} ${source}.d
            DEPENDS ${source}
            DEPFILE ${source}.d
            COMMAND
                ${glslc_executable}
                $<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
                $<$<BOOL:${arg_FORMAT}>:-mfmt=${arg_FORMAT}>
                $<$<BOOL:${arg_ARGS}>:${arg_ARGS}>
                -MD -MF ${source}.d
                -o include/${source}.${arg_EXT}
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        set_source_files_properties(include/${source}.{arg_EXT} PROPERTIES GENERATED TRUE)
        target_sources(${target} PRIVATE include/${source}.${arg_EXT})
    endforeach()
endfunction()

if (MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:preprocessor")
endif()

add_subdirectory("ToyRenderer")
add_subdirectory("utils")
add_subdirectory("libs" SYSTEM)
