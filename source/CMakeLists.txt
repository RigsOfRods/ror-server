FILE(GLOB_RECURSE server_src *.cpp *.c *.h)

# general flags
add_definitions("-D_CRT_SECURE_NO_WARNINGS")
include_directories (${rorserver_SOURCE_DIR}/socketw)

# the final lib
add_executable(rorserver ${server_src})

# libraries
IF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/win32_pthread)
	target_link_libraries(rorserver ${rorserver_SOURCE_DIR}/win32_pthread/pthreadVC2.lib)
	target_link_libraries(rorserver socketw)
ELSEIF(UNIX)
	target_link_libraries(rorserver socketw pthread)
ELSEIF(APPLE)
ENDIF(WIN32)

install(TARGETS rorserver DESTINATION ${rorserver_SOURCE_DIR}/bin)

