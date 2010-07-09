FILE(GLOB_RECURSE server_src *.cpp *.c *.h *.rc)

# general flags
include_directories (${rorserver_SOURCE_DIR}/socketw)

if(WITH_ANGELSCRIPT)
	add_definitions("-DWITH_ANGELSCRIPT")
	include_directories (${rorserver_SOURCE_DIR}/angelscript/include)
	include_directories (${rorserver_SOURCE_DIR}/angelscript/add_on)
endif(WITH_ANGELSCRIPT)

# the final lib
add_executable(rorserver ${server_src})
windows_hacks(rorserver)

# libraries
if(WITH_ANGELSCRIPT)
	add_dependencies(rorserver angelscript)
	target_link_libraries(rorserver angelscript)
endif(WITH_ANGELSCRIPT)

IF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/win32_pthread)
	target_link_libraries(rorserver ${rorserver_SOURCE_DIR}/win32_pthread/pthreadVC2.lib)
	target_link_libraries(rorserver mysocketw)
ELSEIF(UNIX)
	#add_definitions("-DAS_MAX_PORTABILITY")
	target_link_libraries(rorserver mysocketw pthread dl)
ELSEIF(APPLE)
ENDIF(WIN32)

# install the library
INSTALL(TARGETS rorserver
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
)



