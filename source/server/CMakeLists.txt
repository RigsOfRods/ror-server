FILE(GLOB_RECURSE server_src *.cpp *.c *.h *.rc)

# general flags
include_directories(${rorserver_SOURCE_DIR}/dependencies/kissnet)
include_directories(${rorserver_SOURCE_DIR}/dependencies/)
include_directories(${rorserver_SOURCE_DIR}/source/protocol/)
include_directories(${rorserver_SOURCE_DIR}/source/common/)
include_directories(${rorserver_SOURCE_DIR}/dependencies/jsoncpp/include/)

if (RORSERVER_WITH_ANGELSCRIPT)
    add_definitions("-DWITH_ANGELSCRIPT")
    include_directories(${rorserver_SOURCE_DIR}/dependencies/angelscript/include)
    include_directories(${rorserver_SOURCE_DIR}/dependencies/angelscript/add_on)
endif (RORSERVER_WITH_ANGELSCRIPT)

if (RORSERVER_WITH_WEBSERVER)
    add_definitions("-DWITH_WEBSERVER")
    IF (WIN32)
        include_directories(${CMAKE_BINARY_DIR}/dependencies/ctemplate/)
    ELSEIF (UNIX)
        include_directories(${CMAKE_BINARY_DIR}/dependencies/ctemplate/)
    ENDIF (WIN32)
endif (RORSERVER_WITH_WEBSERVER)

# the final lib
set(CMAKE_CXX_STANDARD 17)
add_executable(rorserver ${server_src})

# libraries
find_package(Libevent)
target_include_directories(rorserver PRIVATE ${LIBEVENT_INCLUDE_DIRS})
target_link_libraries(rorserver PRIVATE ${LIBEVENT_LIBRARIES})

if (RORSERVER_WITH_ANGELSCRIPT)
    add_dependencies(rorserver angelscript)
    target_link_libraries(rorserver angelscript)
endif (RORSERVER_WITH_ANGELSCRIPT)

IF (WIN32)
    add_definitions("/wd4251")
    include_directories(${rorserver_SOURCE_DIR}/dependencies/win32_pthread)
    target_link_libraries(rorserver ${rorserver_SOURCE_DIR}/dependencies/win32_pthread/pthreadVC2.lib)
    target_link_libraries(rorserver jsoncpp)
    if (RORSERVER_WITH_WEBSERVER)
        target_link_libraries(rorserver ctemplate)
    endif (RORSERVER_WITH_WEBSERVER)
ELSEIF (UNIX)
    #add_definitions("-DAS_MAX_PORTABILITY")
    target_link_libraries(rorserver pthread dl jsoncpp)
    if (RORSERVER_WITH_WEBSERVER)
        target_link_libraries(rorserver ctemplate)
    endif (RORSERVER_WITH_WEBSERVER)
ELSEIF (APPLE)
ENDIF (WIN32)

# install the library
INSTALL(TARGETS rorserver
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

IF (WIN32)
ELSEIF (UNIX)
    # install the files required for the runtime
    INSTALL(
            DIRECTORY ${rorserver_SOURCE_DIR}/bin/resources/
            DESTINATION share/rorserver/
            FILES_MATCHING PATTERN "*"
            PATTERN ".svn" EXCLUDE
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

    # configure and install init script
    CONFIGURE_FILE(${rorserver_SOURCE_DIR}/contrib/rorserver-initscript.in
            ${rorserver_SOURCE_DIR}/contrib/rorserver-initscript
            @ONLY
            )
    INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/rorserver-initscript
            DESTINATION /etc/init.d
            RENAME rorserver
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server configuration
    INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/example-config.cfg
            DESTINATION /etc/rorserver/
            RENAME simple.cfg.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server authorization file
    INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/example-auth.auth
            DESTINATION /etc/rorserver/
            RENAME simple.auth.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server message of the day (MOTD) file
    INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/example-motd.motd
            DESTINATION /etc/rorserver/
            RENAME simple.motd.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example rules file
    INSTALL(FILES ${rorserver_SOURCE_DIR}/contrib/example-rules.rules
            DESTINATION /etc/rorserver/
            RENAME simple.rules.example
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

    INSTALL(CODE "message(\"example configuration added: /etc/rorserver/simple.cfg.example please configure this file to your likings and rename it to simple.cfg\")")
    INSTALL(CODE "message(\"after you have configured your server, you can start it with: /etc/init.d/rorserver start\")")
ENDIF (WIN32)
