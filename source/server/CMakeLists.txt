FILE(GLOB_RECURSE server_src *.cpp *.c *.h *.rc)

# general flags
include_directories (${rorserver_SOURCE_DIR}/dependencies/socketw)
include_directories (${rorserver_SOURCE_DIR}/dependencies/)

if(RORSERVER_WITH_ANGELSCRIPT)
	add_definitions("-DWITH_ANGELSCRIPT")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/include)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/add_on)
endif(RORSERVER_WITH_ANGELSCRIPT)

if(RORSERVER_WITH_WEBSERVER)
	add_definitions("-DWITH_WEBSERVER")
IF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/ctemplate/src/windows/)
ELSEIF(UNIX)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/ctemplate/src)
ENDIF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/jsoncpp/include/)
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
	target_link_libraries(rorserver mysocketw rudeconfig)
	if(RORSERVER_WITH_WEBSERVER)
		target_link_libraries(rorserver ctemplate jsoncpp)
	endif(RORSERVER_WITH_WEBSERVER)
ELSEIF(UNIX)
	#add_definitions("-DAS_MAX_PORTABILITY")
	target_link_libraries(rorserver mysocketw pthread dl rudeconfig)
	if(RORSERVER_WITH_WEBSERVER)
		target_link_libraries(rorserver ctemplate jsoncpp)
	endif(RORSERVER_WITH_WEBSERVER)
ELSEIF(APPLE)
ENDIF(WIN32)

# install the library
INSTALL(TARGETS rorserver
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

IF(WIN32)
ELSEIF(UNIX)
	# install the files required for the runtime
	INSTALL(
			DIRECTORY ${rorserver_SOURCE_DIR}/bin/webserver
			DESTINATION share/rorserver
			FILES_MATCHING PATTERN "*"
			PATTERN ".svn" EXCLUDE
			PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)

	# configure and install init script
	CONFIGURE_FILE( ${rorserver_SOURCE_DIR}/contrib/rorserver-initscript.in
			${rorserver_SOURCE_DIR}/contrib/rorserver-initscript
			@ONLY
			)
	INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/rorserver-initscript  
			DESTINATION /etc/init.d
			RENAME rorserver
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)

	# create user
	INSTALL(CODE "execute_process(COMMAND useradd -s /bin/false rorserver)")

	# create installation directories
	INSTALL(CODE "execute_process(COMMAND mkdir -p /etc/rorserver)")
	INSTALL(CODE "execute_process(COMMAND mkdir -p /var/log/rorserver)")
	INSTALL(CODE "execute_process(COMMAND mkdir -p /var/run/rorserver)")

	INSTALL(CODE "execute_process(COMMAND chown rorserver:rorserver -R /etc/rorserver)")
	INSTALL(CODE "execute_process(COMMAND chown rorserver:rorserver -R /var/log/rorserver)")
	INSTALL(CODE "execute_process(COMMAND chown rorserver:rorserver -R /var/run/rorserver)")
	
ENDIF(WIN32)


