project(ctemplate)

set(ctemplate_src
	src/base/arena.cc
	src/htmlparser/htmlparser.cc
	src/htmlparser/jsparser.cc
	src/per_expand_data.cc
	src/htmlparser/statemachine.cc
	src/template.cc
	src/template_annotator.cc
	src/template_cache.cc
	src/template_dictionary.cc
	src/template_modifiers.cc
	src/template_namelist.cc
	src/template_pathops.cc
	src/template_string.cc
)

include_directories (src/base)

# libraries
IF(WIN32)
	FILE(GLOB_RECURSE ctemplate_os_src src/windows/*.cc src/windows/ctemplate/*.h )
	include_directories (src/windows/)

	set(ctemplate_src ${ctemplate_src} src/windows/port.cc)
ELSEIF(UNIX)
	include_directories (src/linux/)
	include_directories (src/)
ELSEIF(APPLE)
ENDIF(WIN32)


# general flags
add_definitions("-DLIBCTEMPLATE_EXPORTS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS")

# the final lib
add_library(ctemplate SHARED ${ctemplate_src} ${ctemplate_os_src})
windows_hacks(ctemplate)

# install the library
INSTALL(TARGETS ctemplate
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# install the headers: both source and build headers
INSTALL(
	DIRECTORY ${ctemplate_SOURCE_DIR}/src/ctemplate
	DESTINATION include/ctemplate 
	FILES_MATCHING PATTERN "*.h" 
	PATTERN ".svn" EXCLUDE
	PATTERN "CMakeFiles" EXCLUDE
)

