# ensure cmake features we need
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include(CMakeDependentOption)
include(FeatureSummary)

project(rorserver VERSION "2021.10")

# Options
set(ROR_DEPENDENCY_DIR "${CMAKE_SOURCE_DIR}/dependencies/build" CACHE PATH "Path to the dependencies")
set(CMAKE_PREFIX_PATH ${ROR_DEPENDENCY_DIR} ${CMAKE_PREFIX_PATH})

set(CMAKE_THREAD_PREFER_PTHREAD YES)
find_package(Threads REQUIRED)
find_package(Angelscript)
find_package(jsoncpp REQUIRED)
find_package(SocketW REQUIRED)
find_package(fmt REQUIRED)
find_package(CURL)
cmake_dependent_option(RORSERVER_WITH_ANGELSCRIPT "Adds scripting support" ON "TARGET Angelscript::angelscript" OFF)
cmake_dependent_option(RORSERVER_WITH_CURL "Adds CURL request support (needs AngelScript)" ON "TARGET CURL::libcurl" OFF)

# setup paths
SET(RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
SET(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
SET(ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
SET(EXECUTABLE_OUTPUT_PATH ${RUNTIME_OUTPUT_DIRECTORY})
SET(LIBRARY_OUTPUT_PATH ${RUNTIME_OUTPUT_DIRECTORY})


# hide some settings
mark_as_advanced(
        CMAKE_DEBUG_POSTFIX
        CMAKE_BACKWARDS_COMPATIBILITY
        EXECUTABLE_OUTPUT_PATH
        LIBRARY_OUTPUT_PATH
)

if (WIN32)
    # add multi processor compilation flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}        /MP /GL /Ox /Ob2 /Oi /Ot /Oy /fp:fast /GS- /MP /Zi")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}     /MP /Zi")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MP /Od /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}          /MP /Od /Zi")
    # some general flags
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}          /DEBUG ")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}        /DEBUG /LTCG /OPT:REF")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL}     /DEBUG /LTCG /OPT:REF")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG /LTCG /OPT:REF")
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}          -Og -g -Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}        -O2 -DNDEBUG -Wno-deprecated-declarations -Wno-attributes")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -Wno-deprecated-declarations -Wno-attributes")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}     -Os -DNDEBUG")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")

    # add additional gdb debugging stuff
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -Og -ggdb")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -Og -ggdb")
endif (WIN32)

# fix executable paths for windows
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RUNTIME_OUTPUT_DIRECTORY})


if (UNIX)
    add_definitions(-Wall)
    option(RORSERVER_CRASHHANDLER "enables linux startup script crashhandling" FALSE)

    FIND_PROGRAM(GDB_EXECUTABLE gdb)
    if (NOT GDB_EXECUTABLE)
        message("gdb not found, crashhandler disabled")
        set(RORSERVER_CRASHHANDLER FALSE)
    endif ()
    mark_as_advanced(GDB_EXECUTABLE)
endif ()

if (RORSERVER_WITH_ANGELSCRIPT)
    add_subdirectory("source/angelscript_add_on")
endif ()
add_subdirectory("source/server")

feature_summary(WHAT ALL)