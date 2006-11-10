set(phonon_xineui_PART_SRCS
    videowidget.cpp
    backend.cpp
   )

kde4_automoc(${phonon_xineui_PART_SRCS})
kde4_add_plugin(phonon_xineui ${phonon_xineui_PART_SRCS})
target_link_libraries(phonon_xineui ${KDE_KDEUI_LIBS} phononui phononxineengine)


install(TARGETS phonon_xineui DESTINATION ${PLUGIN_INSTALL_DIR} )
