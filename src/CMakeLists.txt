include(boost.cmake)

SUBDIRS(sha1)
SUBDIRS(files)
SUBDIRS(network)
SUBDIRS(qt-gui)

ADD_EXECUTABLE(uftt
	Main.cpp
	Types.h
	SharedData.cpp
	SharedData.h
)

TARGET_LINK_LIBRARIES(uftt
	uftt-qt
	uftt-lib-network
	boost_thread${BOOST_LIB_SUFFIX}
)