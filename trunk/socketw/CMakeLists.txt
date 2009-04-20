FILE(GLOB_RECURSE socketw_src *.cxx *.h)

# general flags
add_definitions("-DMYSOCKETW_EXPORTS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS")

# the final lib
add_library(socketw SHARED ${socketw_src})

# libraries
IF(WIN32)
	target_link_libraries(socketw Ws2_32.lib)
ELSEIF(UNIX)
ELSEIF(APPLE)
ENDIF(WIN32)

install(TARGETS socketw DESTINATION ${rorserver_SOURCE_DIR}/bin)
