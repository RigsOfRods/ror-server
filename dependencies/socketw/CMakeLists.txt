project(socketw)

find_package(OpenSSL)

set(_HAVE_SSL ${OpenSSL_FOUND})
configure_file(sw_config.h.in sw_config.h)

FILE(GLOB socketw_src *.cxx *.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(socketw STATIC ${socketw_src})

if (OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    target_link_libraries(socketw PRIVATE OpenSSL::SSL)
endif ()

# ignore warnings
set_target_properties(socketw PROPERTIES COMPILE_FLAGS "-w")