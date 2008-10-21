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
	../net-asio/ipaddr_watcher.h
	../net-asio/ipaddr_watcher.cpp
)

SET(NLLIBS "")
IF(UNIX)
	SET(NLLIBS netlink)
ENDIF()

TARGET_LINK_LIBRARIES(bench4
	${BOOST_SYSTEM_LIBRARY}
	${BOOST_THREAD_LIBRARY}
	${BOOST_SIGNALS_LIBRARY}
	${NLLIBS}
)
