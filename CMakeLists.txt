cmake_minimum_required(VERSION 3.27...4.3)
project(miniCDi VERSION 0.11 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(APP_AUTHOR "Mr. Lechkar aka CatmanFan")

include(CMakeDependentOption)

# Debug options
option(MINICDI_DISABLE_VERBOSE "Completely disable verbose logging of any kind." OFF)
cmake_dependent_option(MINICDI_ENABLE_VERBOSE_DISPLAY "Force-enable verbose output display." OFF
                        "NOT MINICDI_DISABLE_VERBOSE" OFF)
option(MINICDI_ENABLE_VERBOSE_DISPLAY_CPU "Print verbose CPU information (PC, SR, FC, D*, A*)." OFF)
option(MINICDI_ENABLE_VERBOSE_OS9 "Print OS-9 syscalls to verbose." OFF)
option(MINICDI_ENABLE_VERBOSE_MODULE "Prefix verbose with OS-9 modules and addresses." OFF)
option(MINICDI_ENABLE_FORCE_LOGFILE "Always use the verbose log to text file option." OFF)

# Emulation options
option(MINICDI_DISABLE_DISC_IFSTREAM "Use C library FILE* instead of std::ifstream for disc sectors." OFF)
option(MINICDI_ENABLE_BENCHMARK "Log duration of frame execution." OFF)

# Target executable & compilation flags
add_executable(${PROJECT_NAME})

if (COMMAND dkp_target_generate_symbol_list)
    dkp_target_generate_symbol_list(${PROJECT_NAME})
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -O3 -fomit-frame-pointer -fno-math-errno -flto=auto -DNDEBUG -Wall -Wno-unused-variable)

# Debug macro definitions
if (MINICDI_DISABLE_VERBOSE)
    message(STATUS "MINICDI_DISABLE_VERBOSE")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_NOLOG)
endif()
if (MINICDI_ENABLE_VERBOSE_DISPLAY)
    message(STATUS "MINICDI_ENABLE_VERBOSE_DISPLAY")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_DEBUG)
    if (MINICDI_ENABLE_VERBOSE_DISPLAY_CPU)
        message(STATUS "MINICDI_ENABLE_VERBOSE_DISPLAY_CPU")
        target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_DEBUG_CPU)
    endif()
endif()
if (MINICDI_ENABLE_VERBOSE_OS9)
    message(STATUS "MINICDI_ENABLE_VERBOSE_OS9")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_DEBUG_OS9)
endif()
if (MINICDI_ENABLE_VERBOSE_MODULE)
    message(STATUS "MINICDI_ENABLE_VERBOSE_MODULE")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_DEBUG_MODULE)
endif()
if (MINICDI_ENABLE_FORCE_LOGFILE)
    message(STATUS "MINICDI_ENABLE_FORCE_LOGFILE")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_FORCE_LOGFILE)
endif()

if (MINICDI_DISABLE_DISC_IFSTREAM)
    message(STATUS "MINICDI_DISABLE_DISC_IFSTREAM")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_NO_DISC_IFSTREAM)
endif()
if (MINICDI_ENABLE_BENCHMARK)
    message(STATUS "MINICDI_ENABLE_BENCHMARK")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_BENCHMARKING)
endif()

# Core source files
target_include_directories(${PROJECT_NAME} PRIVATE src src/cdi/m68k)
file(GLOB_RECURSE MINICDI_SOURCE_FILES LIST_DIRECTORIES false
    src/cdi/*.c
    src/cdi/*.cpp
)
target_sources(${PROJECT_NAME} PRIVATE ${MINICDI_SOURCE_FILES})

find_package(SDL2 REQUIRED)
include_directories(${DEVKITPRO}/portlibs/switch/include ${SDL2_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARIES}
    -lnx
    -lm
)
target_compile_definitions(${PROJECT_NAME} PRIVATE MINICDI_AUDIO=1) # SDL2 audio

# ImGui & Switch frontend
target_include_directories(${PROJECT_NAME} PRIVATE
    src/platforms/nx/imgui
)

target_sources(${PROJECT_NAME} PRIVATE
    src/platforms/nx/imgui/imgui.cpp
    src/platforms/nx/imgui/imgui_demo.cpp
    src/platforms/nx/imgui/imgui_draw.cpp
    src/platforms/nx/imgui/imgui_tables.cpp
    src/platforms/nx/imgui/imgui_widgets.cpp
    src/platforms/nx/imgui/backends/imgui_impl_sdl2.cpp
    src/platforms/nx/imgui/backends/imgui_impl_sdlrenderer2.cpp
    src/frontends/imgui/main.cpp
)

# Package generation (.nacp & .nro)
nx_generate_nacp(${PROJECT_NAME}.nacp
    NAME "${PROJECT_NAME}"
    AUTHOR "${APP_AUTHOR}"
    VERSION ${PROJECT_VERSION}
)

nx_create_nro(${PROJECT_NAME}
    NACP ${PROJECT_NAME}.nacp
    ICON res/nx/icon.png
)
