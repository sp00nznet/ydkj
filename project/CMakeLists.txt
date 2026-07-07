# ydkj - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(ydkj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(YDKJ_SOURCES
    src/main.cpp
)

# ponytail: temporary bring-up harvest. Tolerant indirect dispatcher logs every
# unique unregistered computed-call target (instead of fataling on the first) so
# one play-through harvests them all for batch registration. Toggle OFF for release.
option(YDKJ_HARVEST "Build with tolerant-dispatch harvest scaffold" OFF)
if(YDKJ_HARVEST)
    list(APPEND YDKJ_SOURCES src/dispatch_tolerance.cpp)
endif()

if(WIN32)
    add_executable(ydkj WIN32 ${YDKJ_SOURCES})
else()
    add_executable(ydkj ${YDKJ_SOURCES})
endif()

if(YDKJ_HARVEST AND WIN32)
    target_link_options(ydkj PRIVATE "LINKER:/force:multiple")
endif()

rexglue_setup_target(ydkj)
