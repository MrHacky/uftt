FIND_PACKAGE(MyBoost)

INCLUDE_DIRECTORIES("${BOOST_INCLUDE_DIR}")
LINK_DIRECTORIES("${BOOST_LIBRARY_DIR}")

SUBDIRS(sha1)
SUBDIRS(files)
SUBDIRS(network)
SUBDIRS(qt-gui)

ADD_EXECUTABLE(uftt
	Main.cpp
	Types.h
	SharedData.cpp
	SharedData.h
	JobRequest.cpp
	JobRequest.h
)

TARGET_LINK_LIBRARIES(uftt
	uftt-qt
	uftt-lib-network
	${BOOST_THREAD_LIBRARY}
)