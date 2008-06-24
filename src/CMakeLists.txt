FIND_PACKAGE(MyBoost)
FIND_PACKAGE(OpenSSL)

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
	BuildString.h         BuildStringMacros.cpp
	SimpleBackend.h       SimpleBackend.cpp
	
	SharedData.cpp
	SharedData.h
	JobRequest.cpp
	JobRequest.h
	Logger.cpp
	Logger.h
)

ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/BuildStringMacros.cpp"
 	COMMAND "${CMAKE_COMMAND}"
  	ARGS -E touch "${CMAKE_CURRENT_SOURCE_DIR}/BuildStringMacros.cpp"
 	COMMENT "VerionTarget"
 	DEPENDS uftt
  	)

ADD_CUSTOM_TARGET(VersionTarget ALL 
	   COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildStringMacros.cpp"
	   )

ADD_DEPENDENCIES(uftt VersionTarget)
#	   [COMMAND command2 [args2...] ...]
#	   [DEPENDS depend depend depend ... ]
#	   [WORKING_DIRECTORY dir]
#	   [COMMENT comment] [VERBATIM])


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

	#SET_TARGET_PROPERTIES(uftt PROPERTIES
	#	LINK_FLAGS "/SUBSYSTEM:WINDOWS qtmain.lib"
	#)
	#GET_TARGET_PROPERTY(CURPROPS uftt LINK_FLAGS_RELEASE)
	#MESSAGE(${CURPROPS})
ENDIF(MSVC)

TARGET_LINK_LIBRARIES(uftt
	uftt-qt
	lib-net-asio
	${BOOST_THREAD_LIBRARY}
)

