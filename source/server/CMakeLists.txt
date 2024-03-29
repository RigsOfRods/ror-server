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
    target_link_libraries(${PROJECT_NAME} PRIVATE mysocketw jsoncpp)
ELSEIF (UNIX)
    #add_definitions("-DAS_MAX_PORTABILITY")
    #NOTE: pthread is needed for AngelScript to link.
    target_link_libraries(${PROJECT_NAME} PRIVATE mysocketw pthread dl jsoncpp)
ELSEIF (APPLE)
ENDIF (WIN32)


IF (WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION .)
ELSEIF (UNIX)
    include(GNUInstallDirs)

    # install the library
    INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # configure and install init script
    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/contrib/rorserver-initscript.in
            ${CMAKE_BINARY_DIR}/contrib/rorserver-initscript
            @ONLY
            )
    INSTALL(FILES ${CMAKE_BINARY_DIR}/contrib/rorserver-initscript
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d
            RENAME rorserver
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server configuration
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-config.cfg
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rorserver/
            RENAME simple.cfg.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server authorization file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-auth.auth
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rorserver/
            RENAME simple.auth.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example server message of the day (MOTD) file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-motd.motd
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rorserver/
            RENAME simple.motd.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    # add example rules file
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/contrib/example-rules.rules
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rorserver/
            RENAME simple.rules.example
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )

    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/contrib/postinst.in
            ${CMAKE_BINARY_DIR}/contrib/postinst
            @ONLY
            )
    if(NOT PACKAGING)
        # execute install script
        INSTALL(CODE "execute_process(COMMAND bash ${CMAKE_BINARY_DIR}/contrib/postinst configure )")
    endif()
ENDIF (WIN32)

set(CPACK_PACKAGE_DESCRIPTION "The Rigs of Rods Server is a game server for Rigs of Rods (rigsofrods.org)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dedicated server software for Rigs of Rods")
set(CPACK_PACKAGE_CONTACT "Edgar@AnotherFoxGuy.com")
set(CPACK_PACKAGE_VENDOR "AnotherFoxGuy")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_BINARY_DIR}/contrib/postinst")

if (WIN32)
    set(CPACK_GENERATOR NSIS;ZIP)
elseif (UNIX)
    set(CPACK_GENERATOR DEB;ZIP)
endif ()

include(CPack)