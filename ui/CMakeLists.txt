macro_optional_find_package(XCB)

if(LIBXCB_FOUND AND XINE_XCB_FOUND)
  include_directories(${LIBXCB_INCLUDE_DIR})
  add_definitions(${LIBXCB_DEFINITIONS})

  set(phonon_xineui_PART_SRCS
    videowidget.cpp
    backend.cpp
    )

  kde4_automoc(${phonon_xineui_PART_SRCS})
  kde4_add_plugin(phonon_xineui ${phonon_xineui_PART_SRCS})
  target_link_libraries(phonon_xineui ${KDE4_PHONONUI_LIBS} phononxineengine ${LIBXCB_LIBRARIES})

  install(TARGETS phonon_xineui DESTINATION ${PLUGIN_INSTALL_DIR})
endif(LIBXCB_FOUND AND XINE_XCB_FOUND)
