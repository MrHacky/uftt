FIND_PACKAGE(MyBoost)

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/boost_asio_1_1_0")
INCLUDE_DIRECTORIES("${BOOST_INCLUDE_DIR}")
LINK_DIRECTORIES("${BOOST_LIBRARY_DIR}")

SUBDIRS(sha1)
SUBDIRS(files)
SUBDIRS(net-asio)
SUBDIRS(qt-gui)

ADD_EXECUTABLE(uftt
	Main.cpp
	Types.h
	SharedData.cpp
	SharedData.h
	JobRequest.cpp
	JobRequest.h
	Logger.cpp
	Logger.h
)

ADD_EXECUTABLE(bench
	Benchmark.cpp
	Types.h
	Logger.cpp
	Logger.h
)


TARGET_LINK_LIBRARIES(uftt
	uftt-qt
	lib-net-asio
	${BOOST_THREAD_LIBRARY}
)