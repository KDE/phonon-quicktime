project(Phonon-XineTNG)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroOptionalFindPackage)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${KDE4_INCLUDES} ${XINE_INCLUDE_DIR})

find_package(Xine)

string(REGEX REPLACE "^[0-9]*\\.([0-9]*)\\.[0-9]*$" "\\1" XINE_MINOR_VERSION ${XINE_VERSION})
string(REGEX REPLACE "^[0-9]*\\.[0-9]*\\.([0-9]*)$" "\\1" XINE_BUGFIX_VERSION ${XINE_VERSION})
# if an older xinelib is installed give a warning about problems but continue
if(XINE_MINOR_VERSION LESS 2 AND XINE_BUGFIX_VERSION LESS 90)
   if(XINE_BUGFIX_VERSION LESS 7)
      if(XINE_BUGFIX_VERSION LESS 5)
         macro_log_feature(TRUE "Xine" "xine-lib v${XINE_VERSION} was found on your system. This version does not provide video support and has known security issues. Recommend upgrading to version 1.1.9 or above." "http://sourceforge.net/project/showfiles.php?group_id=9655&package_id=9732")
      else(XINE_BUGFIX_VERSION LESS 5)
         macro_log_feature(TRUE "Xine" "xine-lib v${XINE_VERSION} was found on your system. This version is known to have problems when a device is unplugged. Consider upgrading to version 1.1.9 or above." "http://sourceforge.net/project/showfiles.php?group_id=9655&package_id=9732")
      endif(XINE_BUGFIX_VERSION LESS 5)
   else(XINE_BUGFIX_VERSION LESS 7)
      if(XINE_BUGFIX_VERSION LESS 9)
         macro_log_feature(TRUE "Xine" "xine-lib v${XINE_VERSION} was found on your system. This version is known to have problems when playing short sounds. Consider upgrading to version 1.1.9 or above." "http://sourceforge.net/project/showfiles.php?group_id=9655&package_id=9732")
      endif(XINE_BUGFIX_VERSION LESS 9)
   endif(XINE_BUGFIX_VERSION LESS 7)
else(XINE_MINOR_VERSION LESS 2 AND XINE_BUGFIX_VERSION LESS 90)
   macro_log_feature(FALSE "Xine" "xine-lib v${XINE_VERSION} was found on your system. This version contains major changes compared to 1.1.x and has not been tested properly for use with this backend. Consider downgrading to version 1.1.13." "http://sourceforge.net/project/showfiles.php?group_id=9655&package_id=9732")
endif(XINE_MINOR_VERSION LESS 2 AND XINE_BUGFIX_VERSION LESS 90)

add_subdirectory(kcm)

# xine expects off_t to be 64bits
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

set(phonon_xine_PART_SRCS
   sinknode.cpp
   sourcenode.cpp
   xinethread.cpp
   nullsink.cpp
    xineengine.cpp
    xinestream.cpp
    abstractaudiooutput.cpp
    audiodataoutput.cpp
    effect.cpp
    audiooutput.cpp
    mediaobject.cpp
    #videodataoutput.cpp
    visualization.cpp
    backend.cpp
    volumefadereffect.cpp
    bytestream.cpp
    bytestreamplugin.cpp
    net_buf_ctrl.c
    volumefader_plugin.cpp
    plugins.c
    videowidget.cpp
   )

macro_optional_find_package(XCB)
SET(XCB_VIDEO TRUE)
if(XCB_FOUND AND XINE_XCB_FOUND)
  include_directories(${LIBXCB_INCLUDE_DIR})
  add_definitions(${LIBXCB_DEFINITIONS})
else(XCB_FOUND AND XINE_XCB_FOUND)
  add_definitions(-DPHONON_XINE_NO_VIDEOWIDGET)
  message(STATUS "Cannot compile VideoWidget for the xine backend. Needs xine version 1.1.5 or later and libxcb.")
  SET(XCB_VIDEO FALSE)
endif(XCB_FOUND AND XINE_XCB_FOUND)
macro_log_feature(XCB_VIDEO "XCB" "XCB is needed for the video widget, libxine needs to be compiled with XCB" "http://xcb.freedesktop.org/")

add_definitions(-DPHONON_BACKEND_VERSION_4_2)
kde4_add_plugin(phonon_xine_tng ${phonon_xine_PART_SRCS})
target_link_libraries(phonon_xine_tng ${QT_QTGUI_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_PHONON_LIBS} ${XINE_LIBRARY})
if(XCB_FOUND AND XINE_XCB_FOUND)
  target_link_libraries(phonon_xine_tng ${LIBXCB_LIBRARIES})
endif(XCB_FOUND AND XINE_XCB_FOUND)

install(TARGETS phonon_xine_tng DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES xine_tng.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)
kde4_install_icons(${ICON_INSTALL_DIR})
