cmake_minimum_required(VERSION 3.18)
project(snes9x VERSION 1.62.3)

option(PROFILE "Build for profiling" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "One of: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(PROFILE)
    set(CMAKE_BUILD_TYPE "Debug")
    set(CMAKE_EXE_LINKER_FLAGS "-pg")
    set(CMAKE_C_FLAGS "-O0 -pg -no-pie")
    set(CMAKE_CXX_FLAGS "-O0 -pg -no-pie")
else()
    set(CMAKE_EXE_LINKER_FLAGS "-static -s")
endif()

add_compile_definitions(HAVE_STRINGS_H
                        HAVE_STDINT_H
                        RIGHTSHIFT_IS_SAR
                        )
set(INCLUDES ./apu/bapu ./)
set(SOURCES)
set(ARGS -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-nullability-completeness)
set(LIBS)
set(DEFINES)

include(CheckIncludeFile)

include(FindGettext)

find_package(SDL2 REQUIRED)

list(APPEND ARGS ${SDL2_CFLAGS})
list(APPEND LIBS ${CMAKE_DL_LIBS} SDL2::SDL2-static)

list(APPEND SOURCES
    common/audio/s9x_sound_driver.hpp
    filter/2xsai.cpp
    filter/2xsai.h
    filter/epx.cpp
    filter/epx.h
    filter/snes_ntsc_config.h
    filter/snes_ntsc.h
    filter/snes_ntsc_impl.h
    filter/snes_ntsc.c
    common/audio/s9x_sound_driver_sdl.hpp
    common/audio/s9x_sound_driver_sdl.cpp
    fxinst.cpp
    fxemu.cpp
    fxdbg.cpp
    c4.cpp
    c4emu.cpp
    apu/apu.cpp
    apu/bapu/dsp/sdsp.cpp
    apu/bapu/smp/smp.cpp
    apu/bapu/smp/smp_state.cpp
    msu1.cpp
    msu1.h
    dsp.cpp
    dsp1.cpp
    dsp2.cpp
    dsp3.cpp
    dsp4.cpp
    spc7110.cpp
    obc1.cpp
    seta.cpp
    seta010.cpp
    seta011.cpp
    seta018.cpp
    controls.cpp
    crosshairs.cpp
    cpu.cpp
    sa1.cpp
    debug.cpp
    sdd1.cpp
    tile.cpp
    tileimpl-n1x1.cpp
    tileimpl-n2x1.cpp
    tileimpl-h2x1.cpp
    srtc.cpp
    gfx.cpp
    memmap.cpp
    clip.cpp
    ppu.cpp
    dma.cpp
    snes9x.cpp
    globals.cpp
    stream.cpp
    conffile.cpp
    bsx.cpp
    snapshot.cpp
    screenshot.cpp
    movie.cpp
    statemanager.cpp
    sha256.cpp
    bml.cpp
    cpuops.cpp
    cpuexec.cpp
    sa1cpu.cpp
    cheats.cpp
    cheats2.cpp
    sdd1emu.cpp
    netplay.cpp
    server.cpp
    loadzip.cpp
    fscompat.cpp
    )

list(APPEND SOURCES external/fmt/src/format.cc)
list(APPEND INCLUDES external/fmt/include)

list(APPEND SOURCES sdl.cpp)

add_executable(snes9x ${SOURCES})
target_include_directories(snes9x PRIVATE ${INCLUDES})
target_compile_options(snes9x PRIVATE ${ARGS})
target_link_libraries(snes9x PRIVATE ${LIBS})
target_compile_definitions(snes9x PRIVATE ${DEFINES})

install(TARGETS snes9x)