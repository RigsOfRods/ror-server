project(jsoncpp)

FILE(GLOB_RECURSE jsoncpp_src src/lib_json/*.cpp)
include_directories (include)



# general flags
#add_definitions("-DLIBCTEMPLATE_EXPORTS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS")

# the final lib
add_library(jsoncpp STATIC ${jsoncpp_src} ${ctemplate_os_src})
windows_hacks(jsoncpp)

# install the library
INSTALL(TARGETS jsoncpp
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

# install the headers: both source and build headers
INSTALL(
	DIRECTORY ${ctemplate_SOURCE_DIR}/include/json
	DESTINATION include/jsoncpp 
	FILES_MATCHING PATTERN "*.h" 
	PATTERN ".svn" EXCLUDE
	PATTERN "CMakeFiles" EXCLUDE
)

