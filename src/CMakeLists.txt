FIND_PACKAGE(MyBoost)

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
	${BOOST_THREAD_LIBRARY}
)