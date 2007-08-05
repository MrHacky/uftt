include(boost.cmake)

SUBDIRS(files)
SUBDIRS(network)
SUBDIRS(qt-gui)
SUBDIRS(sha1)

ADD_EXECUTABLE(uftt
	Main.cpp
)

TARGET_LINK_LIBRARIES(uftt
	uftt-qt
)