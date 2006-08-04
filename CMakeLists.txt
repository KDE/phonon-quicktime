

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

# add_subdirectory( ui )

########### next target ###############

set(phonon_xine_PART_SRCS
   xine_engine.cpp
   abstractmediaproducer.cpp
   abstractaudiooutput.cpp
   audiodataoutput.cpp
   audioeffect.cpp
   audiooutput.cpp
   audiopath.cpp
   bytestream.cpp
   mediaobject.cpp
   avcapture.cpp
   videoeffect.cpp
   videopath.cpp
   videodataoutput.cpp
   visualization.cpp
   backend.cpp
   volumefadereffect.cpp )

kde4_automoc(${phonon_xine_PART_SRCS})

kde4_add_plugin(phonon_xine ${phonon_xine_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} phonon_xine )

target_link_libraries(phonon_xine ${KDE4_KDECORE_LIBS} phononcore ${XINE_LIBRARY} )

install(TARGETS phonon_xine DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install(FILES xine.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)
