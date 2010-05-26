ADD_EXECUTABLE(bench1
	Benchmark.cpp
	../Types.h
	../Logger.h
)

TARGET_LINK_LIBRARIES(bench1
	${BOOST_SYSTEM_LIBRARY}
	${BOOST_THREAD_LIBRARY}
)

ADD_EXECUTABLE(bench2
	Bench2.cpp
	../Types.h
	../Logger.h
)

TARGET_LINK_LIBRARIES(bench2
	${BOOST_SYSTEM_LIBRARY}
	${BOOST_THREAD_LIBRARY}
)

ADD_EXECUTABLE(bench4
	Bench4.cpp
	../linux/libnetlink.c ../linux/libnetlink.h
	../net-asio/ipaddr_watcher.h
	../net-asio/ipaddr_watcher.cpp
)

TARGET_LINK_LIBRARIES(bench4
	${BOOST_SYSTEM_LIBRARY}
	${BOOST_THREAD_LIBRARY}
	${BOOST_SIGNALS_LIBRARY}
)

ADD_EXECUTABLE(bench5
	Bench5.cpp
	../Types.h
	../Logger.h
	../net-asio/asio_file_stream.cpp
	../util/Filesystem.cpp
	../Platform.cpp
)

TARGET_LINK_LIBRARIES(bench5
	${BOOST_SYSTEM_LIBRARY}
	${BOOST_THREAD_LIBRARY}
	${BOOST_FILESYSTEM_LIBRARY}
)
