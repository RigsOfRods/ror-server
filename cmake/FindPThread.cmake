# Find PThread
# ----------------
#
# Find PThread include directories and libraries.
# This module will set the following variables:
#
# * PThread_FOUND       - True if PThread is found
# * PThread_INCLUDE_DIR - The include directory
# * PThread_LIBRARY     - The libraries to link against
#
# In addition the following imported targets are defined:
#
# * PThread::PThread
#

find_path(PThread_INCLUDE_DIR pthread.h)
find_library(PThread_LIBRARY libpthreadVC3 PATH_SUFFIXES Debug Release)

set(PThread_INCLUDE_DIRS ${PThread_INCLUDE_DIR})
set(PThread_LIBRARIES ${PThread_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        PThread FOUND_VAR PThread_FOUND
        REQUIRED_VARS PThread_INCLUDE_DIRS PThread_LIBRARIES
)

if (PThread_FOUND)
    add_library(PThread::PThread INTERFACE IMPORTED)
    set_target_properties(PThread::PThread PROPERTIES
            INTERFACE_LINK_LIBRARIES "${PThread_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${PThread_INCLUDE_DIRS}"
            )
endif ()

mark_as_advanced(PThread_INCLUDE_DIR PThread_LIBRARY)
