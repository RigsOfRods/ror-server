FILE(GLOB_RECURSE socketw_src *.cxx *.h)

# general flags
add_definitions("-DMYSOCKETW_EXPORTS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS")

# the final lib
add_library(mysocketw SHARED ${socketw_src})

# libraries
IF(WIN32)
	target_link_libraries(mysocketw Ws2_32.lib)
ELSEIF(UNIX)
ELSEIF(APPLE)
ENDIF(WIN32)

install(TARGETS mysocketw DESTINATION ${rorserver_SOURCE_DIR}/bin)
