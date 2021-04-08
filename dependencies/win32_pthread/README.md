# pthread-win ~ POSIX Threads (pthreads) for Windows

This is an adaptation of the library [pthread-win32](https://github.com/GerHobbelt/pthread-win32.git) for CMake.

# Example
```js
cmake_minimum_required(VERSION 2.8)
include(ExternalProject)

project(example)

set(externals_prefix "${CMAKE_CURRENT_BINARY_DIR}/externals")
set(externals_install_dir "${CMAKE_CURRENT_BINARY_DIR}/libs")

set_property(DIRECTORY PROPERTY EP_BASE ${externals_prefix})
ExternalProject_Add(lpthread
    GIT_REPOSITORY "https://github.com/Puasonych/pthread-win.git"
    GIT_TAG "master"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${externals_install_dir} -DPTHREAD_BUILD_SHARED=OFF
)

include_directories(${externals_install_dir}/include)
set(PTHREAD_LIB "${externals_install_dir}/lib/libpthread.lib")
add_executable(${PROJECT_NAME} main.cpp)
add_dependencies(${PROJECT_NAME} lpthread)
target_link_libraries(
    ${PROJECT_NAME}
    ${PTHREAD_LIB}
)
```
