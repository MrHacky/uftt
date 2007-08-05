include(boost.cmake)

SUBDIRS(sha1)
SUBDIRS(files)
SUBDIRS(network)
SUBDIRS(qt-gui)

ADD_EXECUTABLE(uftt
	Main.cpp
)

TARGET_LINK_LIBRARIES(uftt
	uftt-qt
)