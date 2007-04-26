add_subdirectory(ui)
add_subdirectory(kcm)

include_directories(${XINE_INCLUDE_DIR})

set(xineengine_LIB_SRCS
    xineengine.cpp
    xinestream.cpp
    audioport.cpp
    audiopostlist.cpp
   )
kde4_automoc(${xineengine_LIB_SRCS})
kde4_add_library(phononxineengine SHARED ${xineengine_LIB_SRCS})
target_link_libraries(phononxineengine ${XINE_LIBRARY} ${KDE4_KAUDIODEVICELIST_LIBS})
set_target_properties(phononxineengine PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})
install(TARGETS phononxineengine DESTINATION ${LIB_INSTALL_DIR})

# xine expects off_t to be 64bits
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

set(phonon_xine_PART_SRCS
    abstractmediaproducer.cpp
    abstractaudiooutput.cpp
    audiodataoutput.cpp
    audioeffect.cpp
    audiooutput.cpp
    audiopath.cpp
    bytestream.cpp
    mediaobject.cpp
    mediaqueue.cpp
    avcapture.cpp
    videoeffect.cpp
    brightnesscontrol.cpp
    deinterlacefilter.cpp
    videopath.cpp
    videodataoutput.cpp
    visualization.cpp
    backend.cpp
    volumefadereffect.cpp
    bytestreamplugin.cpp
    net_buf_ctrl.c
    volumefader_plugin.cpp
    plugins.c
   )

kde4_automoc(${phonon_xine_PART_SRCS})
kde4_add_plugin(phonon_xine ${phonon_xine_PART_SRCS})
target_link_libraries(phonon_xine ${KDE4_KDECORE_LIBS} ${KDE4_PHONONCORE_LIBS} phononxineengine)


install(TARGETS phonon_xine DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES xine.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)
kde4_install_icons( ${ICON_INSTALL_DIR} )
