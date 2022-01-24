project(jsoncpp)

FILE(GLOB_RECURSE jsoncpp_src src/lib_json/*.cpp)
include_directories(include)

# the final lib
add_library(jsoncpp STATIC ${jsoncpp_src})

# ignore warnings
set_target_properties(jsoncpp PROPERTIES COMPILE_FLAGS "-w")

if (NOT WIN32)
    # install the library
    INSTALL(TARGETS jsoncpp
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()