# The C++ side of the golden comparison: libultragui on the host platform, in
# draw-data mode, with a clock the scenario sets (golden_platform.cc).
#
#   cmake -S aki/tools/golden -B /tmp/ugui-golden -G Ninja && ninja -C /tmp/ugui-golden
cmake_minimum_required(VERSION 3.20)
project(ugui_golden CXX C)

set(ULTRAGUI_PLATFORM_HOST ON CACHE BOOL "" FORCE)
set(ULTRAGUI_PLATFORM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/golden_platform.cc"
    CACHE FILEPATH "" FORCE)
set(ULTRAGUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ULTRAGUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ULTRAGUI_AUDIO OFF CACHE BOOL "" FORCE)
set(ULTRAGUI_LOTTIE OFF CACHE BOOL "" FORCE)
set(ULTRAGUI_VIDEO OFF CACHE BOOL "" FORCE)
option(UGUI_GOLDEN_LUA "Run the scenarios' Lua scripts" ON)
set(ULTRAGUI_LUA ${UGUI_GOLDEN_LUA} CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ultragui)

add_executable(ugui_golden ugui_golden.cc)
target_link_libraries(ugui_golden PRIVATE ultragui)
