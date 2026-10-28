cmake_minimum_required(VERSION 3.16)
project(point_of_no_return C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(GD_FETCH_SDL "Download and build SDL3 from GitHub if it is not installed" ON)

set(GD_SOURCES
    src/main.c
    src/game.c
    src/level.c
    src/physics.c
    src/bot.c
    src/audio.c
    src/font.c
    src/util.c
    src/assets.c
)

if(ANDROID)
    # The Android build (android/app/jni/CMakeLists.txt) already provides SDL3::SDL3.
    add_library(main SHARED ${GD_SOURCES})
    target_include_directories(main PRIVATE src third_party)
    target_link_libraries(main PRIVATE SDL3::SDL3)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(main PRIVATE -Wall -Wextra)
    endif()
    return()
endif()

find_package(SDL3 CONFIG QUIET)

if(NOT SDL3_FOUND AND NOT GD_FETCH_SDL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    endif()
endif()

if(NOT TARGET SDL3::SDL3 AND GD_FETCH_SDL AND NOT SDL3_FOUND)
    message(STATUS "SDL3 not found, fetching it from GitHub")
    include(FetchContent)

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)

    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.30
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_executable(point_of_no_return ${GD_SOURCES})
target_include_directories(point_of_no_return PRIVATE src third_party)

if(TARGET SDL3::SDL3)
    target_link_libraries(point_of_no_return PRIVATE SDL3::SDL3)
else()
    target_link_libraries(point_of_no_return PRIVATE PkgConfig::SDL3)
endif()

target_link_libraries(point_of_no_return PRIVATE m)

if(MSVC)
    target_compile_definitions(point_of_no_return PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(point_of_no_return PRIVATE -Wall -Wextra)
    target_link_options(point_of_no_return PRIVATE -Wl,--no-undefined)
endif()

# install: binary + assets next to each other
include(GNUInstallDirs)
install(TARGETS point_of_no_return RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_DATADIR}/point_of_no_return)

# the level verification runs after every build:  ctest --output-on-failure
enable_testing()
add_test(NAME level_selftest
         COMMAND point_of_no_return --selftest
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
