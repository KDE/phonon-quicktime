

########### next target ###############

set(phonon_fakeui_PART_SRCS videowidget.cpp backend.cpp )

kde4_automoc(${phonon_fakeui_PART_SRCS})

kde4_add_plugin(phonon_fakeui ${phonon_fakeui_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} phonon_fakeui )

target_link_libraries(phonon_fakeui  ${KDE_KIO_LIBS} phononui )

install_targets(${PLUGIN_INSTALL_DIR} phonon_fakeui )


########### install files ###############


