FILE(GLOB_RECURSE server_src *.cpp *.c *.h *.rc)

# general flags
include_directories (${rorserver_SOURCE_DIR}/dependencies/socketw)

if(RORSERVER_WITH_ANGELSCRIPT)
	add_definitions("-DWITH_ANGELSCRIPT")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/include)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/add_on)
endif(RORSERVER_WITH_ANGELSCRIPT)

if(RORSERVER_WITH_WEBSERVER)
	add_definitions("-DWITH_WEBSERVER")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/ctemplate/src/windows/)
endif(RORSERVER_WITH_WEBSERVER)

# the final lib
add_executable(rorserver ${server_src})
windows_hacks(rorserver)

# libraries
if(RORSERVER_WITH_ANGELSCRIPT)
	add_dependencies(rorserver angelscript)
	target_link_libraries(rorserver angelscript)
endif(RORSERVER_WITH_ANGELSCRIPT)

IF(WIN32)
	add_definitions("/wd4251")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/win32_pthread)
	target_link_libraries(rorserver ${rorserver_SOURCE_DIR}/dependencies/win32_pthread/pthreadVC2.lib)
	target_link_libraries(rorserver mysocketw)
	if(RORSERVER_WITH_WEBSERVER)
		target_link_libraries(rorserver ctemplate)
	endif(RORSERVER_WITH_WEBSERVER)
ELSEIF(UNIX)
	#add_definitions("-DAS_MAX_PORTABILITY")
	target_link_libraries(rorserver mysocketw pthread dl)
ELSEIF(APPLE)
ENDIF(WIN32)

# install the library
INSTALL(TARGETS rorserver
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)



