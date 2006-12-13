# - Try to find the XINE  library
# Once done this will define
#
#  XINE_FOUND - system has the XINE library
#  XINE_INCLUDE_DIR - the XINE include directory
#  XINE_LIBRARIES - The libraries needed to use XINE

# Copyright (c) 2006, Laurent Montel, <montel@kde.org>
# Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (XINE_INCLUDE_DIR AND XINE_LIBRARIES)
  # Already in cache, be silent
  set(XINE_FIND_QUIETLY TRUE)
endif (XINE_INCLUDE_DIR AND XINE_LIBRARIES)

FIND_PATH(XINE_INCLUDE_DIR xine.h
 /usr/include/
 /usr/local/include/
)


FIND_LIBRARY(XINE_LIBRARY NAMES xine
 PATHS
 /usr/lib
 /usr/local/lib
)

FIND_PROGRAM(XINECONFIG_EXECUTABLE NAMES xine-config PATHS
   /usr/bin
   /usr/local/bin
)

if (XINE_INCLUDE_DIR AND XINE_LIBRARY AND XINECONFIG_EXECUTABLE)
   EXEC_PROGRAM(${XINECONFIG_EXECUTABLE} ARGS --version RETURN_VALUE _return_VALUE OUTPUT_VARIABLE XINE_VERSION)
   macro_ensure_version(1.1.1 ${XINE_VERSION} XINE_VERSION_OK)
   if (XINE_VERSION_OK)
      set(XINE_FOUND TRUE)
   endif (XINE_VERSION_OK)
endif (XINE_INCLUDE_DIR AND XINE_LIBRARY AND XINECONFIG_EXECUTABLE)


if (XINE_FOUND)
   if (NOT XINE_FIND_QUIETLY)
      message(STATUS "Found XINE: ${XINE_LIBRARY}")
   endif (NOT XINE_FIND_QUIETLY)
   if(XINECONFIG_EXECUTABLE)
      EXEC_PROGRAM(${XINECONFIG_EXECUTABLE} ARGS --plugindir RETURN_VALUE _return_VALUE OUTPUT_VARIABLE XINEPLUGINSDIR)
      MESSAGE(STATUS "XINEPLUGINSDIR :<${XINEPLUGINSDIR}>")
   endif(XINECONFIG_EXECUTABLE)
else (XINE_FOUND)
   if (XINE_FIND_REQUIRED)
      message(FATAL_ERROR "Could NOT find XINE 1.1.1 or greater")
   endif (XINE_FIND_REQUIRED)
endif (XINE_FOUND)

MARK_AS_ADVANCED(XINE_INCLUDE_DIR XINE_LIBRARY XINEPLUGINSDIR)
