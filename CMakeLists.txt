######### begin: remove when moved back to kdemultimedia ############
project(phonon-xine-threaded)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)
include (MacroLogFeature)
find_package(Xine)

add_definitions (${QT_DEFINITIONS} ${QT_QTDBUS_DEFINITIONS} ${KDE4_DEFINITIONS})
link_directories (${KDE4_LIB_DIR})
include_directories (${QDBUS_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

#configure_file (config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
######### end: remove when moved back to kdemultimedia ##############

#add_subdirectory(ui)
#add_subdirectory(xineplugins)

set(xineengine_LIB_SRCS
    xineengine.cpp
   )
kde4_automoc(${xineengine_LIB_SRCS})
kde4_add_library(phononxineengine SHARED ${xineengine_LIB_SRCS})
target_link_libraries(phononxineengine ${QT_QTCORE_LIBRARY} ${XINE_LIBRARY} ${KDE4_KAUDIODEVICELIST_LIBS} ${KDE4_SOLID_LIBS})
set_target_properties(phononxineengine PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})
install(TARGETS phononxineengine DESTINATION ${LIB_INSTALL_DIR})

# xine expects off_t to be 64bits
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

set(phonon_xine_PART_SRCS
    audioport.cpp
    xinestream.cpp
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
kde4_add_plugin(phonon_xine_threaded ${phonon_xine_PART_SRCS})
target_link_libraries(phonon_xine_threaded ${KDE4_KDECORE_LIBS} ${KDE4_PHONONCORE_LIBS} phononxineengine)


install(TARGETS phonon_xine_threaded DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES xinet.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)
