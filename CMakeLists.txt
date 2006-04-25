kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

# add_subdirectory( ui ) 

########### next target ###############

set(phonon_xine_PART_SRCS 
   abstractmediaproducer.cpp 
   abstractaudiooutput.cpp 
   audiodataoutput.cpp 
   delayaudioeffect.cpp
   audioeffect.cpp 
   audiooutput.cpp 
   audiopath.cpp 
   bytestream.cpp 
   mediaobject.cpp 
   avcapture.cpp 
   videoeffect.cpp 
   videopath.cpp 
   backend.cpp 
   volumefadereffect.cpp )

kde4_automoc(${phonon_xine_PART_SRCS})

kde4_add_plugin(phonon_xine ${phonon_xine_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} phonon_xine )

target_link_libraries(phonon_xine ${KDE4_KDECORE_LIBS} phononcore xine )

install_targets(${PLUGIN_INSTALL_DIR} phonon_xine )


########### install files ###############

install_files( ${SERVICES_INSTALL_DIR}/phononbackends FILES xine.desktop )

kde4_footer()
