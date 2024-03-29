FILE(GLOB_RECURSE socketw_src *.cxx *.h)

project(socketw)

# general flags
add_definitions("-DMYSOCKETW_EXPORTS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS")

# the final lib
add_library(mysocketw SHARED ${socketw_src})

# ignore warnings
set_target_properties(mysocketw PROPERTIES COMPILE_FLAGS "-w")

# libraries
IF (WIN32)
    target_link_libraries(mysocketw Ws2_32.lib)
ELSEIF (UNIX)
ELSEIF (APPLE)
ENDIF (WIN32)

if (WIN32)
    install(TARGETS mysocketw RUNTIME DESTINATION .)
else ()
    # install the library
    INSTALL(TARGETS mysocketw
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()