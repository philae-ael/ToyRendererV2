find_package(spdlog REQUIRED)
add_library(utils STATIC
        src/assert.cpp
        src/timer.cpp
        include/utils/assert.h
        include/utils/types.h
        include/utils/math.h
        include/utils/cast.h
        include/utils/timer.h
        include/utils/misc.h
        include/utils/data/hive.h
        include/utils/data/sparse_set.h
        include/utils/data/static_stack.h
)

target_include_directories(utils PUBLIC "include/")
target_link_libraries(utils PRIVATE spdlog::spdlog)

if(MSVC)
    target_compile_options(utils PRIVATE /Wall /WX)
else()
    target_compile_options(utils PRIVATE -Wall -Wextra -pedantic)
endif()
