cmake_minimum_required(VERSION 3.16)

project(Game)

# SDL3 sources.  Clone them here first (see android/README or scripts/fetch_sdl.sh):
#   git clone --depth 1 -b release-3.2.30 https://github.com/libsdl-org/SDL android/app/jni/SDL
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL3 sources are missing. Run scripts/fetch_sdl.sh (or: git clone --depth 1 "
        "-b release-3.2.30 https://github.com/libsdl-org/SDL android/app/jni/SDL)")
endif()

add_subdirectory(SDL)

# The game itself lives in the repository root.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. gd_game)
