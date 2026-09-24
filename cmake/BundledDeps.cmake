# freetype, harfbuzz and Lua 5.4 from pinned release archives, for platforms
# with no pkg-config to ask (Windows, cross builds). Included by the top-level
# CMakeLists when ULTRAGUI_BUNDLED_DEPS is ON.
#
# Offline, point FETCHCONTENT_SOURCE_DIR_UGUI_FREETYPE (and _HARFBUZZ, _LUA)
# at unpacked copies of the same releases.

include(FetchContent)
# freetype and Lua are C.
enable_language(C)

if(NOT TARGET freetype)
    FetchContent_Declare(ugui_freetype
        URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz
        URL_HASH SHA256=bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # The glyph rasterizer only: fonts come as files or memory, not zipped.
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ugui_freetype)
endif()

if(NOT TARGET harfbuzz)
    FetchContent_Declare(ugui_harfbuzz
        URL https://github.com/harfbuzz/harfbuzz/releases/download/10.1.0/harfbuzz-10.1.0.tar.xz
        URL_HASH SHA256=6ce3520f2d089a33cef0fc48321334b8e0b72141f6a763719aaaecd2779ecb82
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        # Unpacked only; its own CMakeLists is not used (see below).
        SOURCE_SUBDIR ugui-sources-only
    )
    FetchContent_MakeAvailable(ugui_harfbuzz)
    # The amalgamated translation unit, which spares harfbuzz's own build
    # from having to find the freetype built just above.
    add_library(harfbuzz STATIC ${ugui_harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
    target_include_directories(harfbuzz PUBLIC ${ugui_harfbuzz_SOURCE_DIR}/src)
    target_compile_definitions(harfbuzz PRIVATE HAVE_FREETYPE=1)
    target_compile_options(harfbuzz PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(harfbuzz PUBLIC freetype)
endif()

if(ULTRAGUI_LUA AND NOT TARGET ultragui_lua)
    FetchContent_Declare(ugui_lua
        URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
        URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR ugui-sources-only
    )
    FetchContent_MakeAvailable(ugui_lua)
    file(GLOB _lua_sources ${ugui_lua_SOURCE_DIR}/src/*.c)
    # The interpreter and compiler programs, not the library.
    list(REMOVE_ITEM _lua_sources
        ${ugui_lua_SOURCE_DIR}/src/lua.c
        ${ugui_lua_SOURCE_DIR}/src/luac.c)
    add_library(ultragui_lua STATIC ${_lua_sources})
    set_target_properties(ultragui_lua PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(ultragui_lua PUBLIC ${ugui_lua_SOURCE_DIR}/src)
endif()
