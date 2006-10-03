find_package(Threads)

ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

SET(xineplug_inp_bytestream_LIB_SRCS
	input_bytestream.cpp
	plugin.c
	)

ADD_LIBRARY(xineplug_inp_bytestream SHARED ${xineplug_inp_bytestream_LIB_SRCS})
TARGET_LINK_LIBRARIES(xineplug_inp_bytestream ${XINE_LIBRARY} ${QT_QTCORE_LIBRARY} ${KDE4_KDECORE_LIBS})
SET_TARGET_PROPERTIES(xineplug_inp_bytestream PROPERTIES PREFIX "")
INSTALL(TARGETS xineplug_inp_bytestream DESTINATION ${XINEPLUGINSDIR})
