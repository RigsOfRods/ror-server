#
# Native ctemplate CMake build.
#

cmake_minimum_required( VERSION 2.8 )
set( CTEMPLATE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" )
if( NOT CTEMPLATE_SOURCE_ROOT )
  message( FATAL_ERROR "CTEMPLATE_SOURCE_ROOT not found, it must be set before including the ctemplate CMake build." )
endif()

project( ctemplate )

# Enable the C++11 option for GNU compilers.
if(CMAKE_COMPILER_IS_GNUCXX)
  add_definitions(-std=c++11)
endif()

find_package( PythonInterp 2.7 REQUIRED )

include( CheckIncludeFiles )

# Set variables required for the generation of 'config.h'.
set( GOOGLE_NAMESPACE "::ctemplate" )
set( _START_GOOGLE_NAMESPACE_ "namespace ctemplate {" )
set( _END_GOOGLE_NAMESPACE_ "} // namespace ctemplate" )
set( HASH_MAP_H "<unordered_map>" )
set( HASH_NAMESPACE "std" )
set( HASH_SET_H "<unordered_set>" )
set( HAVE_HASH_MAP 1 )
set( HAVE_HASH_SET 1 )
check_include_files("memory.h" HAVE_MEMORY_H )
set( HAVE_NAMESPACES 1 )
check_include_files("stdlib.h" HAVE_STDLIB_H )
check_include_files("string.h" HAVE_STRING_H )
check_include_files("sys/stat.h" HAVE_SYS_STAT_H )
check_include_files("sys/types.h" HAVE_SYS_TYPES_H )
check_include_files("stdint.h" HAVE_STDINT_H )
set( HAVE_UINT32_T 1 )
check_include_files("unistd.h" HAVE_UNISTD_H )
set( HAVE_UNORDERED_MAP 1 )
if( WIN32 )
  set( HAVE___INT32 1 )
  set( HAVE___INT64 1 )
else()
  set( HAVE_UINT64_T 1 )
endif()
set( HTMLPARSER_NAMESPACE google_ctemplate_streamhtmlparser )
set( PACKAGE_STRING "\"ctemplate 1.1\"" )
if( WIN32 )
  set( PRIdS  "\"Id\"" )
  set( PRIuS  "\"Iu\"" )
  set( PRIxS  "\"Ix\"" )
else()
  set( PRIdS  "\"ld\"" )
  set( PRIuS  "\"lu\"" )
  set( PRIxS  "\"lx\"" )
endif()
set( STDC_HEADERS 1 )
set( STL_NAMESPACE std )

# Set variables required for the generation of other header files
set( ac_google_namespace ${GOOGLE_NAMESPACE} )
set( ac_google_start_namespace ${_START_GOOGLE_NAMESPACE_} )
set( ac_google_end_namespace ${_END_GOOGLE_NAMESPACE_} )
set( ac_cv_cxx_hash_map ${HASH_MAP_H} )
set( ac_cv_cxx_hash_set ${HASH_SET_H} )
set( ac_cv_cxx_hash_map_class std::unordered_map )
set( ac_cv_cxx_hash_set_class std::unordered_set )
set( ac_cv_have_inttypes_h 0 )
set( ac_cv_have_stdint_h ${HAVE_STDINT_H} )
set( ac_have_attribute_weak 0 )
set( ac_google_attribute 0 )
set( ac_htmlparser_namespace google_ctemplate_streamhtmlparser )
if( WIN32 )
  set( ac_cv_uint64 __int64 )
else()
  set( ac_cv_uint64 uint64_t )
endif()

# Generate 'config.h'
configure_file( config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY )

# On windows builds make a copy of config.h that resides in the
# windows folder. Some of ctemplate's include files require one to be there and
# it needs to be the same as the one generated.
if( WIN32 )
  configure_file( config.h.in "${CMAKE_CURRENT_BINARY_DIR}/windows/config.h" @ONLY )
endif()

# Generate other header files
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_string.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_string.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_enums.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_enums.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_cache.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_cache.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_modifiers.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_modifiers.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_emitter.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_emitter.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_annotator.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_annotator.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_dictionary.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_dictionary.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_pathops.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_pathops.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_namelist.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_namelist.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/find_ptr.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/find_ptr.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/per_expand_data.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/per_expand_data.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/str_ref.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/str_ref.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/str_ref.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/str_ref.h"
  @ONLY )
configure_file(
  "${CTEMPLATE_SOURCE_ROOT}/src/ctemplate/template_dictionary_interface.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctemplate/template_dictionary_interface.h"
  @ONLY )

# Add custom commands for generation of headers via. a python script.
add_custom_command(
  OUTPUT htmlparser/htmlparser_fsm.h
  COMMAND
    ${CMAKE_COMMAND} -E make_directory htmlparser
  COMMAND
    ${PYTHON_EXECUTABLE}
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/generate_fsm.py
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/htmlparser_fsm.config
    > htmlparser/htmlparser_fsm.h
  MAIN_DEPENDENCY
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/htmlparser_fsm.config
  DEPENDS 
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/generate_fsm.py )
add_custom_command(
  OUTPUT htmlparser/jsparser_fsm.h
  COMMAND
    ${CMAKE_COMMAND} -E make_directory htmlparser
  COMMAND
    ${PYTHON_EXECUTABLE}
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/generate_fsm.py
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/jsparser_fsm.config
    > htmlparser/jsparser_fsm.h
  MAIN_DEPENDENCY
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/jsparser_fsm.config
  DEPENDS 
    ${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/generate_fsm.py )

# Create a custom target that depends on the python generated headers.
add_custom_target(
  CTemplatePythonGeneratedFiles
  DEPENDS
    htmlparser/htmlparser_fsm.h
    htmlparser/jsparser_fsm.h )

# Create the 'ctemplate' target with appropriate dependencies, definitions, and
# includes.
add_library( ctemplate STATIC 
	${CTEMPLATE_SOURCE_ROOT}/src/base/arena.cc
	${CTEMPLATE_SOURCE_ROOT}/src/per_expand_data.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_annotator.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_cache.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_dictionary.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_modifiers.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_namelist.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_pathops.cc
	${CTEMPLATE_SOURCE_ROOT}/src/template_string.cc
	${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/htmlparser.cc
	${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/jsparser.cc
	${CTEMPLATE_SOURCE_ROOT}/src/htmlparser/statemachine.cc )
add_dependencies( ctemplate CTemplatePythonGeneratedFiles )
add_definitions( -DNO_THREADS )
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CTEMPLATE_SOURCE_ROOT}/src )
