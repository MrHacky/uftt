
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
)
