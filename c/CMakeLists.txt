cmake_minimum_required(VERSION 3.16)

project(reaper_lua_osc_c
    VERSION 0.1.0
    LANGUAGES C
)

add_library(reaper_lua_osc SHARED
    src/reaper_lua_osc.c
)

target_include_directories(reaper_lua_osc
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(reaper_lua_osc PUBLIC c_std_11)

# Reasonable default warnings
if(MSVC)
    target_compile_options(reaper_lua_osc PRIVATE /W4)
else()
    target_compile_options(reaper_lua_osc PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(reaper_lua_osc PROPERTIES
    OUTPUT_NAME "reaper_lua_osc"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Optional smoke test executable
option(REAPER_LUA_OSC_BUILD_TEST "Build a small link/run test" ON)

if(REAPER_LUA_OSC_BUILD_TEST)
    add_executable(reaper_lua_osc_smoke
        tests/smoke.c
    )
    target_link_libraries(reaper_lua_osc_smoke PRIVATE reaper_lua_osc)
    target_compile_features(reaper_lua_osc_smoke PRIVATE c_std_11)
endif()
