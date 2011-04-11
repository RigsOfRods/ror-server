project(rudeconfig)

FILE(GLOB_RECURSE rudeconfig_src src/*.cpp)
include_directories (include)

# general flags
#add_definitions("")

# the final lib
add_library(rudeconfig STATIC ${rudeconfig_src})

# install the library
INSTALL(TARGETS rudeconfig
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# install the headers: both source and build headers
INSTALL(
	DIRECTORY ${rudeconfig_SOURCE_DIR}/src/
	DESTINATION include/rudeconfig
	FILES_MATCHING PATTERN "config.h" 
	PATTERN ".svn" EXCLUDE
	PATTERN "CMakeFiles" EXCLUDE
)

