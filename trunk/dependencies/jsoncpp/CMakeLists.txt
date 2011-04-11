project(jsoncpp)

FILE(GLOB_RECURSE jsoncpp_src src/lib_json/*.cpp)
include_directories (include)

# the final lib
add_library(jsoncpp STATIC ${jsoncpp_src})

# install the library
INSTALL(TARGETS jsoncpp
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# install the headers: both source and build headers
INSTALL(
	DIRECTORY ${jsoncpp_SOURCE_DIR}/include/json
	DESTINATION include/jsoncpp 
	FILES_MATCHING PATTERN "*.h" 
	PATTERN ".svn" EXCLUDE
	PATTERN "CMakeFiles" EXCLUDE
)

