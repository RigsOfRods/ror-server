project(jsoncpp)

include_directories(json)

# the final lib
add_library(jsoncpp STATIC jsoncpp.cpp)

# ignore warnings
set_target_properties(jsoncpp PROPERTIES COMPILE_FLAGS "-w")