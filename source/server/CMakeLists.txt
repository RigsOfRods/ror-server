FILE(GLOB_RECURSE server_src CONFIGURE_DEPENDS *.cpp *.c *.h *.rc)

# the final lib
add_executable(${PROJECT_NAME} ${server_src})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/dependencies/socketw/
        ${CMAKE_SOURCE_DIR}/dependencies/
        ${CMAKE_SOURCE_DIR}/source/protocol/
        ${CMAKE_SOURCE_DIR}/source/common/
        ${CMAKE_SOURCE_DIR}/dependencies/jsoncpp/include/
        )

# libraries
if (RORSERVER_WITH_ANGELSCRIPT)
    add_dependencies(${PROJECT_NAME} angelscript)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_ANGELSCRIPT)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/angelscript/include)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/angelscript/add_on)
    target_link_libraries(${PROJECT_NAME} PRIVATE angelscript)
endif ()

IF (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PTW32_STATIC_LIB)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/win32_pthread/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE mysocketw jsoncpp pthread)
ELSEIF (UNIX)
    #add_definitions("-DAS_MAX_PORTABILITY")
    target_link_libraries(${PROJECT_NAME} PRIVATE mysocketw pthread dl jsoncpp)
ELSEIF (APPLE)
ENDIF (WIN32)


IF (WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION .)
ELSEIF (UNIX)
    # install the library
    INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
    )
    # install the files required for the runtime
    INSTALL(
            DIRECTORY ${CMAKE_SOURCE_DIR}/bin/resources/
            DESTINATION share/rorserver/
            FILES_MATCHING PATTERN "*"
            PATTERN ".svn" EXCLUDE
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

    # configure and install init script
    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/contrib/rorserver-initscript.in
            ${CMAKE_SOURCE_DIR}/contrib/rorserver-initscript
            @ONLY
            )
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/rorserver-initscript
            DESTINATION /etc/init.d
            RENAME rorserver
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server configuration
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-config.cfg
            DESTINATION /etc/rorserver/
            RENAME simple.cfg.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server authorization file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-auth.auth
            DESTINATION /etc/rorserver/
            RENAME simple.auth.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server message of the day (MOTD) file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-motd.motd
            DESTINATION /etc/rorserver/
            RENAME simple.motd.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example rules file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-rules.rules
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
