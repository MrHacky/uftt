FIND_PACKAGE(MyBoost)
FIND_PACKAGE(OpenSSL)

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/boost_asio_1_1_0")
INCLUDE_DIRECTORIES("${BOOST_INCLUDE_DIR}")
LINK_DIRECTORIES("${BOOST_LIBRARY_DIR}")

SUBDIRS(sha1)
SUBDIRS(files)
SUBDIRS(net-asio)
SUBDIRS(qt-gui)

SET(UFTT_SOURCES
	Main.cpp

	Types.h
	BuildString.h         BuildVersion.inc
	SimpleBackend.h       SimpleBackend.cpp
	
	guicon.cpp            guicon.h
	qt-gui/Icons/icon.rc
	
	SharedData.cpp
	SharedData.h
	JobRequest.cpp
	JobRequest.h
	Logger.cpp
	Logger.h
)

SET(UFTT_LIBRARIES
	uftt-qt
	lib-net-asio
)

ADD_EXECUTABLE(uftt ${UFTT_SOURCES} BuildStringMacros.cpp)

ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/BuildStringMacros.cpp"
	COMMAND "${CMAKE_COMMAND}"
	ARGS -E touch "${CMAKE_CURRENT_SOURCE_DIR}/BuildStringMacros.cpp"
	COMMENT "VerionTarget"
	DEPENDS ${UFTT_SOURCES} ${UFTT_LIBRARIES}
	)

if(OPENSSL_FOUND)
	add_definitions("-DUSE_OPENSSL")
	INCLUDE_DIRECTORIES("${OPENSSL_INCLUDE_DIR}")
	TARGET_LINK_LIBRARIES(uftt
		${OPENSSL_LIBRARY}
		${EAY32_LIBRARY}
	)
endif(OPENSSL_FOUND)

ADD_EXECUTABLE(bench
	Benchmark.cpp
	Types.h
	Logger.h
)

IF(MSVC)
	SET_TARGET_PROPERTIES(uftt PROPERTIES
		LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS"
	)
ENDIF(MSVC)

TARGET_LINK_LIBRARIES(uftt
	${BOOST_THREAD_LIBRARY}
	${UFTT_LIBRARIES}
)

