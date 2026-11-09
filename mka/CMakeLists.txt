cmake_minimum_required(VERSION 3.20)

# ============================================================================
# YOUR_GAME_NAME - Static Recompilation
#
# Copy this template to your game project root and customize:
#   1. Change the project name below
#   2. Set XBOXRECOMP_DIR to where you cloned xboxrecomp
#   3. Adjust source file lists as you add game-specific code
#   4. Add any game-specific compile definitions or link libraries
# ============================================================================

project(mka_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    # MKA's image (with .data BSS) ends at 0x00959640, past the runtime's fixed
    # regions at 0x00700000-0x00F80000. Move them up 3 MB, above the image, and
    # trim the main stack to 5 MB so the heap still starts at 0x00F80000.
    XBOX_RUNTIME_AREA_SHIFT=0x00300000
    XBOX_STACK_SIZE=0x00500000
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
#
# Option A: xboxrecomp as a subdirectory (simplest)
#   Place xboxrecomp/ next to your game project and use a relative path.
#
# Option B: find_package (if you install xboxrecomp system-wide)
#   find_package(xboxrecomp REQUIRED)
#
# Adjust this path to point to your xboxrecomp clone:
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

# Add xboxrecomp as a subdirectory -- this builds all 6 runtime libraries
# and provides the 'xboxrecomp' umbrella target to link against.
# By default the runtime looks for the DSP56300 core as if it sat in third_party/ itself; here
# xboxrecomp is beside mka/, and the core in third_party/dsp56300.
set(DSP56300_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/dsp56300" CACHE PATH
    "Pinned DSP56300 source checkout")
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# ── Recompiled game executable ──────────────────────────────────

# Collect generated recompiled code.
# The recomp pipeline outputs files into src/recomp/gen/.
# With --split 1000, you get gen_0000.c, gen_0001.c, etc.
# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed files
# without a manual re-configure. A plain GLOB is evaluated once at configure
# time, so a newly generated file (an extra chunk, recomp_stubs_unresolved.c)
# is silently left out of the build and shows up as unresolved externals.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

# Core game sources
set(GAME_SOURCES
    src/main.c
    src/recomp_manual.c
    src/mka_process.c   # Midway process coroutines on host fibers
)

# The recomp pipeline also generates a dispatch header and function declarations.
# These are included by the generated .c files:
#   src/recomp/gen/recomp_funcs.h    - extern declarations for all recompiled functions
#   src/recomp/recomp_dispatch.h     - binary search dispatch table
#   src/recomp/recomp_types.h        - register model, ICALL macros, MEM helpers

add_executable(${PROJECT_NAME} WIN32
    ${GAME_SOURCES}
    ${RECOMP_GEN_SOURCES}
    launcher/mka.rc
)

# Link against the xboxrecomp umbrella target (all 6 runtime libraries)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

# Windows SDK libraries needed at link time
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    dxguid
    xinput
    winmm
    # Lets the fault handler resolve the host RIP back to the generated
    # function's symbol, i.e. to the guest function that faulted.
    dbghelp
)

# MSVC-specific: large address aware, increase section limit for big generated files
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
    )
    # The generated code files can be very large. Increase object file section limit.
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    # Symbols for the runtime libraries (not the generated code, which would
    # make every rebuild far longer), so the sampling profiler
    # (RECOMP_KGPU_PROFILE) and crash reports can name GPU/APU/kernel functions.
    foreach(_lib xbox_kernel xbox_nv2a xbox_apu xbox_video xbox_d3d8 xbox_input xbox_usb)
        if(TARGET ${_lib})
            target_compile_options(${_lib} PRIVATE /Zi)
        endif()
    endforeach()
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()

# ── Game-specific additions ─────────────────────────────────────
#
# Add any game-specific libraries, loaders, or custom renderers below.
# Examples:
#
# # Custom texture loader for your game's format
# target_sources(${PROJECT_NAME} PRIVATE src/texture_loader.c)
#
# # If your game uses RenderWare, you may need a RW bridge layer
# target_sources(${PROJECT_NAME} PRIVATE src/rw_bridge.c)
#
# # Additional compile definitions for debugging
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     ENABLE_ICALL_TRACE=1
#     VERBOSE_KERNEL_LOG=1
# )

# ── Settings launcher ───────────────────────────────────────────
#
# The settings window (disc and installation, video, game, controls); it writes
# video.ini / launcher.ini and starts the game with run.cmd's environment.
# Copied next to run.cmd as "MK Armageddon.exe".
add_executable(mka_launcher WIN32 launcher/launcher.c launcher/launcher.rc)
target_link_libraries(mka_launcher PRIVATE comctl32 comdlg32 shell32 user32 advapi32)
if(MSVC)
    target_compile_options(mka_launcher PRIVATE /utf-8)
endif()
# and to the package's top folder, where the starter ZIP has it (it finds mka\ beside it)
add_custom_command(TARGET mka_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mka_launcher> "${CMAKE_CURRENT_SOURCE_DIR}/MK Armageddon.exe"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mka_launcher> "${CMAKE_CURRENT_SOURCE_DIR}/../MK Armageddon.exe")
