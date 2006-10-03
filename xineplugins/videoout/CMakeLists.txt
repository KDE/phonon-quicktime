########### next target ###############

include_directories(${X11_XV_INCLUDE_DRIS})

find_package(Threads)
if(X11_XV_FOUND)
	SET(xineplug_vo_out_xv_nt_LIB_SRCS
		video_out_xv.c
		x11osd.c
		deinterlace.c
		)

	ADD_LIBRARY(xineplug_vo_out_xv_nt SHARED ${xineplug_vo_out_xv_nt_LIB_SRCS})
	TARGET_LINK_LIBRARIES(xineplug_vo_out_xv_nt ${XINE_LIBRARY} ${X11_LIBRARIES} ${X11_Xv_LIB} ${CMAKE_THREAD_LIBS_INIT})
	SET_TARGET_PROPERTIES(xineplug_vo_out_xv_nt PROPERTIES PREFIX "")
	INSTALL(TARGETS xineplug_vo_out_xv_nt DESTINATION ${XINEPLUGINSDIR})
endif(X11_XV_FOUND)

#xineplug_vo_out_xv_la_SOURCES = $(X11OSD) deinterlace.c video_out_xv.c
#xineplug_vo_out_xv_la_LIBADD = $(XV_LIB) $(X_LIBS) -lXext $(XINE_LIB) $(THREAD_LIBS)
#xineplug_vo_out_xv_la_LDFLAGS = -avoid-version -module @XINE_PLUGIN_MIN_SYMS@

########### install files ###############
