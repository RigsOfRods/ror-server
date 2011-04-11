# ensure cmake features we need
CMAKE_MINIMUM_REQUIRED( VERSION 2.6.0 )
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
# done with cmake check

project(rorserver)

# setup paths
SET(RUNTIME_OUTPUT_DIRECTORY "${rorserver_SOURCE_DIR}/bin/")
SET(LIBRARY_OUTPUT_DIRECTORY "${rorserver_SOURCE_DIR}/lib/")
SET(ARCHIVE_OUTPUT_DIRECTORY "${rorserver_SOURCE_DIR}/lib/")
SET(EXECUTABLE_OUTPUT_PATH   ${RUNTIME_OUTPUT_DIRECTORY})
SET(LIBRARY_OUTPUT_PATH      ${RUNTIME_OUTPUT_DIRECTORY})

# hide some settings
mark_as_advanced(
  CMAKE_DEBUG_POSTFIX
  CMAKE_BACKWARDS_COMPATIBILITY
  EXECUTABLE_OUTPUT_PATH
  LIBRARY_OUTPUT_PATH
)

if(WIN32)
  # add multi processor compilation flags
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        /MP /GL /Ox /Ob2 /Oi /Ot /Oy /fp:fast /GS- /MP /Zi")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}     /MP /Zi")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MP /Od /Zi")
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}          /MP /Od /Zi")
  # some general flags
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")

  set(CMAKE_EXE_LINKER_FLAGS_DEBUG          "${CMAKE_EXE_LINKER_FLAGS_DEBUG}          /DEBUG ")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE        "${CMAKE_EXE_LINKER_FLAGS_RELEASE}        /DEBUG /LTCG /OPT:REF")
  set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL     "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL}     /DEBUG /LTCG /OPT:REF")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG /LTCG /OPT:REF")
ELSEIF(UNIX)
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE        "${CMAKE_EXE_LINKER_FLAGS_RELEASE}      -mfpmath=sse -msse2 -mmmx -msse -msse3 -m3dnow -O2 -fomit-frame-pointer -fstrict-aliasing -ffast-math -ftracer")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -O0")

  # add gdb debugging stuff
  set(CMAKE_CXX_FLAGS_DEBUG                 "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG          "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -O0 -ggdb")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG       "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -O0 -ggdb")
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG       "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -O0 -ggdb")
endif(WIN32)

# fix executable paths for windows
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RUNTIME_OUTPUT_DIRECTORY})

# some additional compiler flags
IF(NOT WIN32)
	ADD_DEFINITIONS(-Wall)
endif()

#options
option(RORSERVER_GUI "build server GUI, experimental" FALSE)
option(RORSERVER_WITH_ANGELSCRIPT "adds scripting support" FALSE)
option(RORSERVER_WITH_WEBSERVER "adds the webserver" FALSE)


IF(NOT WIN32)
	option(RORSERVER_CRASHHANDLER "enables linux startup script crashhandling" FALSE)

	FIND_PROGRAM(GDB_EXECUTABLE gdb)
	IF(NOT GDB_EXECUTABLE)
		message("gdb not found, crashhandler disabled")
		set(RORSERVER_CRASHHANDLER FALSE)
	ENDIF()
	mark_as_advanced(GDB_EXECUTABLE)
endif()


add_subdirectory("dependencies/socketw")
add_subdirectory("dependencies/rudeconfig")
add_subdirectory("source/server")

if(RORSERVER_WITH_ANGELSCRIPT)
	add_subdirectory("dependencies/angelscript")
endif(RORSERVER_WITH_ANGELSCRIPT)

if(RORSERVER_GUI)
	add_subdirectory("source/gui")
endif(RORSERVER_GUI)

if(RORSERVER_WITH_WEBSERVER)
	add_subdirectory("dependencies/ctemplate")
	add_subdirectory("dependencies/jsoncpp")
endif(RORSERVER_WITH_WEBSERVER)
