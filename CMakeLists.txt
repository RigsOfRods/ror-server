project(rorserver)

cmake_minimum_required(VERSION 2.6)

option(BUILD_GUI "build server GUI, experimental" FALSE)

add_subdirectory("socketw")
add_subdirectory("source")
add_subdirectory("angelscript")

if(BUILD_GUI)
add_subdirectory("servergui")
endif(BUILD_GUI)
