project(servergui)

FILE(GLOB_RECURSE servergui_src *.cpp *.c *.h *.rc)

FILE(GLOB_RECURSE servergui_src ${rorserver_SOURCE_DIR}/source/*.cpp ${rorserver_SOURCE_DIR}/source/*.c ${rorserver_SOURCE_DIR}/source/*.h )

SET(wxWidgets_USE_UNICODE ON)
IF(WIN32)
 SET(wxWidgets_USE_STATIC ON)
ENDIF(WIN32)

FIND_PACKAGE(wxWidgets COMPONENTS base core html net)
IF(wxWidgets_FOUND)
  include_directories(${wxWidgets_INCLUDE_DIRS})
  include_directories (${rorserver_SOURCE_DIR}/socketw)
  include_directories (${rorserver_SOURCE_DIR}/source)
  add_definitions(${wxWidgets_DEFINITIONS} ${wxWidgets_CXX_FLAGS})

  IF(WIN32)
    include_directories(${wxWidgets_INCLUDE_DIRS}/msvc)
    add_definitions("-D__WIN32__")
  ENDIF(WIN32)
  link_directories (${wxWidgets_LIB_DIR})
  include_directories (${rorserver_SOURCE_DIR}/)

  add_definitions("-D_UNICODE -D_CRT_SECURE_NO_WARNINGS -DNOCMDLINE")

  # always use no debug, otherwise we cannot link against the wxWidgets lib (that used no debug mode on compile time)
  add_definitions("-DNDEBUG")

  #add_definitions("${CMAKE_WXWINDOWS_CXX_FLAGS}")


  add_executable(servergui ${servergui_src})
  TARGET_LINK_LIBRARIES(servergui ${wxWidgets_LIBRARIES} )
  IF(WIN32)
	include_directories (${rorserver_SOURCE_DIR}/win32_pthread)
	target_link_libraries(servergui ${rorserver_SOURCE_DIR}/win32_pthread/pthreadVC2.lib)
	target_link_libraries(servergui mysocketw)
  ELSEIF(UNIX)
	target_link_libraries(servergui mysocketw pthread)
  ELSEIF(APPLE)
  ENDIF(WIN32)
  
  IF(WIN32)
    SET_TARGET_PROPERTIES(servergui PROPERTIES COMPILE_FLAGS "")
    SET_TARGET_PROPERTIES(servergui PROPERTIES LINK_FLAGS    "/SUBSYSTEM:WINDOWS")  
  ENDIF(WIN32)

  install(TARGETS servergui DESTINATION ${rorserver_SOURCE_DIR}/bin)

ELSE(wxWidgets_FOUND)
    MESSAGE("wxWidgets not found. Will not build the configurator.")
ENDIF(wxWidgets_FOUND)
