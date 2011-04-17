project(servergui)

FILE(GLOB_RECURSE servergui_src *.cpp *.c *.h *.rc)

FILE(GLOB_RECURSE servergui_server_src 
	${rorserver_SOURCE_DIR}/source/server/*.cpp 
	${rorserver_SOURCE_DIR}/source/server/*.c 
	${rorserver_SOURCE_DIR}/source/server/*.h )

add_definitions(-DWITHOUTMAIN)

SET(wxWidgets_USE_UNICODE ON)
IF(WIN32)
 SET(wxWidgets_USE_STATIC ON)
ENDIF(WIN32)

FIND_PACKAGE(wxWidgets COMPONENTS base core html net)
IF(wxWidgets_FOUND)
  include_directories(${wxWidgets_INCLUDE_DIRS})
  include_directories (${rorserver_SOURCE_DIR}/dependencies/socketw)
  include_directories (${rorserver_SOURCE_DIR}/dependencies)
  include_directories (${rorserver_SOURCE_DIR}/source/server/)
  include_directories (${rorserver_SOURCE_DIR}/source/protocol/)

  if(RORSERVER_WITH_ANGELSCRIPT)
	add_definitions("-DWITH_ANGELSCRIPT")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/include)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/angelscript/add_on)
  endif(RORSERVER_WITH_ANGELSCRIPT)
  
  if(RORSERVER_WITH_WEBSERVER)
	add_definitions("-DWITH_WEBSERVER")
	include_directories (${rorserver_SOURCE_DIR}/dependencies/ctemplate/src/windows/)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/jsoncpp/include/)
  endif(RORSERVER_WITH_WEBSERVER)


  add_definitions("-D${wxWidgets_DEFINITIONS} -D${wxWidgets_CXX_FLAGS}")

  IF(WIN32)
	add_definitions("/wd4251")
    include_directories(${wxWidgets_INCLUDE_DIRS}/msvc)
    add_definitions("-D__WIN32__")
  ENDIF(WIN32)
  link_directories (${wxWidgets_LIB_DIR})
  include_directories (${rorserver_SOURCE_DIR}/)

  add_definitions("-DNOCMDLINE")

  # always use no debug, otherwise we cannot link against the wxWidgets lib (that used no debug mode on compile time)
  add_definitions("-DNDEBUG")

  #add_definitions("${CMAKE_WXWINDOWS_CXX_FLAGS}")

  add_executable(servergui WIN32 ${servergui_src} ${servergui_server_src})

  set_target_properties(servergui PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
  set_target_properties(servergui PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
  set_target_properties(servergui PROPERTIES LINK_FLAGS_RELEASE "/LTCG /OPT:REF /SUBSYSTEM:WINDOWS")

  if(RORSERVER_WITH_ANGELSCRIPT)
    add_dependencies(servergui angelscript)
    target_link_libraries(servergui angelscript)
  endif(RORSERVER_WITH_ANGELSCRIPT)

  if(RORSERVER_WITH_WEBSERVER)
	target_link_libraries(servergui ctemplate jsoncpp)
  endif(RORSERVER_WITH_WEBSERVER)
  
  TARGET_LINK_LIBRARIES(servergui ${wxWidgets_LIBRARIES} )
  IF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/dependencies/win32_pthread)
	target_link_libraries(servergui ${rorserver_SOURCE_DIR}/dependencies/win32_pthread/pthreadVC2.lib)
	target_link_libraries(servergui mysocketw rudeconfig)
  ELSEIF(UNIX)
	target_link_libraries(servergui mysocketw pthread rudeconfig)
  ELSEIF(APPLE)
  ENDIF(WIN32)

  IF(WIN32)
    SET_TARGET_PROPERTIES(servergui PROPERTIES COMPILE_FLAGS "")
    SET_TARGET_PROPERTIES(servergui PROPERTIES LINK_FLAGS    "/SUBSYSTEM:WINDOWS")
  ENDIF(WIN32)

  # install the library
  INSTALL(TARGETS servergui
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  )

ELSE(wxWidgets_FOUND)
    MESSAGE("wxWidgets not found. Will not build the configurator.")
ENDIF(wxWidgets_FOUND)
