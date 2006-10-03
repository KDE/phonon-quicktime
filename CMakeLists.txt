add_subdirectory(ui)
#add_subdirectory(xineplugins)

set(xine_engine_LIB_SRCS
    xine_engine.cpp
   )
kde4_automoc(${xine_engine_LIB_SRCS})
kde4_add_library(phononxineengine SHARED ${xine_engine_LIB_SRCS})
target_link_libraries(phononxineengine ${QT_QTCORE_LIBRARY} ${XINE_LIBRARY})
set_target_properties(phononxineengine PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})
install(TARGETS phononxineengine DESTINATION ${LIB_INSTALL_DIR})

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
    videopath.cpp
    videodataoutput.cpp
    visualization.cpp
    backend.cpp
    volumefadereffect.cpp
    xineplugins/bytestream/input_bytestream.cpp
    xineplugins/bytestream/plugin.c
   )

kde4_automoc(${phonon_xine_PART_SRCS})
kde4_add_plugin(phonon_xine ${phonon_xine_PART_SRCS})
target_link_libraries(phonon_xine ${KDE4_KDECORE_LIBS} phononcore phononxineengine)

kde4_install_libtool_file(${PLUGIN_INSTALL_DIR} phonon_xine)
install(TARGETS phonon_xine DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES xine.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)
