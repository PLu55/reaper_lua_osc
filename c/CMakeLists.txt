cmake_minimum_required(VERSION 3.16)

project(reaper_lua_osc_c
    VERSION 0.1.0
    LANGUAGES C
)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBLO REQUIRED liblo)

find_package(Lua 5.4 REQUIRED)

option(REAPER_LIBLO_LINK_LUA "Link against liblua (useful for standalone builds; REAPER may provide Lua symbols)" OFF)

# If set, copy the built module into your REAPER scripts folder so `require("reaper_liblo")` works.
set(REAPER_LIBLO_DEPLOY_DIR "" CACHE PATH "Deploy directory for reaper_liblo.so (e.g. ~/.config/REAPER/Scripts/plums/core)")

add_library(reaper_liblo SHARED
    src/reaper_liblo.c
)

target_include_directories(reaper_liblo
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${LIBLO_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
)

target_link_directories(reaper_liblo PRIVATE ${LIBLO_LIBRARY_DIRS})
target_link_libraries(reaper_liblo PRIVATE ${LIBLO_LIBRARIES})

if(REAPER_LIBLO_LINK_LUA)
    target_link_libraries(reaper_liblo PRIVATE ${LUA_LIBRARIES})
endif()

target_compile_features(reaper_liblo PUBLIC c_std_11)

# Reasonable default warnings
if(MSVC)
    target_compile_options(reaper_liblo PRIVATE /W4)
else()
    target_compile_options(reaper_liblo PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(reaper_liblo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "reaper_liblo"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(REAPER_LIBLO_DEPLOY_DIR)
    add_custom_command(TARGET reaper_liblo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REAPER_LIBLO_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:reaper_liblo>"
        "${REAPER_LIBLO_DEPLOY_DIR}/reaper_liblo.so"
        VERBATIM
    )
endif()

# Optional smoke test executable
option(REAPER_LIBLO_BUILD_TEST "Build a small link/run test" ON)

if(REAPER_LIBLO_BUILD_TEST)
    add_executable(reaper_liblo_smoke
        tests/smoke.c
    )
    target_link_libraries(reaper_liblo_smoke PRIVATE reaper_liblo)
    target_compile_features(reaper_liblo_smoke PRIVATE c_std_11)

    target_include_directories(reaper_liblo_smoke PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(reaper_liblo_smoke PRIVATE ${LUA_LIBRARIES})
endif()
