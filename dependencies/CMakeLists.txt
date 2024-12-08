# The MIT License (MIT)
#
# Copyright (c) 2016 Fabian Killus
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.7)


# ------------------------------------------------------------------------------------------------#
# The intention of this script is to provide a simple solution for building all dependencies
# required for Rigs of Rods. It is in fact a super- or meta-build project which makes use of
# the ExternalProject module of CMake to build the individual dependencies.
# ------------------------------------------------------------------------------------------------#

project(ror-server-dependencies VERSION 1)


# ------------------------------------------------------------------------------------------------#
# Options to enable/disable building specific dependencies
# ------------------------------------------------------------------------------------------------#

# Options with same default value on all platforms
option(BUILD_ANGELSCRIPT "Build AngelScript scripting library" ON)
option(BUILD_SOCKETW "Build SocketW networking library" ON)
option(BUILD_JSONCPP "Build JsonCpp JSON parser library" ON)

# Options with different default values on Windows and Linux
include(CMakeDependentOption)
cmake_dependent_option(BUILD_PTHREAD_WIN "Build pthread-win32 for Windows" ON WIN32 OFF)

# ------------------------------------------------------------------------------------------------#
# General setup and initialization
# ------------------------------------------------------------------------------------------------#

include(ExternalProject)

# Set the EP_BASE directory property to setup the build directory structure (see the
# ExternalProject documentation for more information)
set_property(DIRECTORY PROPERTY EP_BASE ${CMAKE_BINARY_DIR})


# Determine the name for the output directory where dependencies are going to be installed
if (WIN32)
    set(DEPENDENCIES_OUTPUT_DIR ${CMAKE_BINARY_DIR}/Dependencies_${CMAKE_SYSTEM_NAME}_${CMAKE_GENERATOR})
    string(REPLACE " " "-" DEPENDENCIES_OUTPUT_DIR ${DEPENDENCIES_OUTPUT_DIR})
else ()
    set(DEPENDENCIES_OUTPUT_DIR ${CMAKE_BINARY_DIR}/Dependencies_${CMAKE_SYSTEM_NAME})
endif ()

if (MSVC)
    set(DEPENDENCIES_INCLUDE_DIR ${DEPENDENCIES_OUTPUT_DIR}/include)
    set(DEPENDENCIES_LIB_DIR ${DEPENDENCIES_OUTPUT_DIR}/lib)
    set(DEPENDENCIES_BIN_DIR ${DEPENDENCIES_OUTPUT_DIR}/bin)
else ()
    set(DEPENDENCIES_INCLUDE_DIR ${DEPENDENCIES_OUTPUT_DIR}/include)
    set(DEPENDENCIES_LIB_DIR ${DEPENDENCIES_OUTPUT_DIR}/lib/${CMAKE_BUILD_TYPE})
    set(DEPENDENCIES_BIN_DIR ${DEPENDENCIES_OUTPUT_DIR}/bin/${CMAKE_BUILD_TYPE})
endif ()

file(MAKE_DIRECTORY ${DEPENDENCIES_INCLUDE_DIR})
file(MAKE_DIRECTORY ${DEPENDENCIES_LIB_DIR})
file(MAKE_DIRECTORY ${DEPENDENCIES_BIN_DIR})


# Compile flags
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /w")
endif ()

if (UNIX)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wno-deprecated-declarations -Wno-attributes")
endif ()


# On Linux some packages rely on pkgconfig to be found correctly. Therefore the environment variable
# needs to be set up accordingly. In addition when building depedencies which need to find other
# dependencies (e.g. MyGUI searching for OGRE) PKG_CONFIG_USE_CMAKE_PREFIX_PATH needs to be set to TRUE.
if (UNIX)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${DEPENDENCIES_LIB_DIR}/pkgconfig")
endif ()


# ------------------------------------------------------------------------------------------------#
#  JsonCpp
# ------------------------------------------------------------------------------------------------#
if (BUILD_JSONCPP)
    ExternalProject_Add(
            jsoncpp
            GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp
            GIT_TAG 5defb4ed1a4293b8e2bf641e16b156fb9de498cc
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_OUTPUT_DIR}
            -DJSONCPP_WITH_EXAMPL=NO
            -DJSONCPP_WITH_POST_BUILD_UNITTEST=NO
            -DJSONCPP_WITH_TESTS=NO
            -DCMAKE_BUILD_TYPE=$<CONFIG>
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    )
endif ()

# ------------------------------------------------------------------------------------------------#
#  SocketW
# ------------------------------------------------------------------------------------------------#

if (BUILD_SOCKETW)
    ExternalProject_Add(
            socketw
            GIT_REPOSITORY https://github.com/RigsOfRods/socketw.git
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_OUTPUT_DIR}
            -DCMAKE_BUILD_TYPE=$<CONFIG>
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    )
endif ()


# ------------------------------------------------------------------------------------------------#
#  Angelscript
# ------------------------------------------------------------------------------------------------#

if (BUILD_ANGELSCRIPT)
    ExternalProject_Add(
            angelscript
            URL http://prdownloads.sourceforge.net/rigs-of-rods/angelscript_2.35.1.zip
            URL_HASH SHA256=5c1096b6d6cf50c7e77ae93c736d35b69b07b1e5047161c7816bca25b413a18b
            SOURCE_SUBDIR angelscript/projects/cmake
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_OUTPUT_DIR}
            -DCMAKE_BUILD_TYPE=$<CONFIG>
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    )
endif ()

# ------------------------------------------------------------------------------------------------#
#  pthread-win32
# ------------------------------------------------------------------------------------------------#

if (BUILD_PTHREAD_WIN)
    ExternalProject_Add(
            pthread
            GIT_REPOSITORY https://github.com/GerHobbelt/pthread-win32.git
            GIT_TAG bd0cc9a3caa795378bf157895b882e0cd9aa8c4e
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_OUTPUT_DIR}
            -DENABLE_TESTS=NO
            -DDLLDEST=bin
            -DLIBDEST=lib
            -DHDRDEST=include
            -DTESTDEST=test
            -DCMAKE_BUILD_TYPE=$<CONFIG>
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    )
endif ()