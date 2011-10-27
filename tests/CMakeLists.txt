Set(TESTING_BASH "bash" CACHE STRING "Location of the bash shell")
Set(TESTING_BASH_ARGS "" CACHE STRING "Extra arguments for the bash shell")

Set(TESTING_DEFAULT_TARGETS "")
If(ENABLE_QT_GUI)
	Set(TESTING_DEFAULT_TARGETS "${TESTING_DEFAULT_TARGETS};uftt")
Endif()
If(ENABLE_GTK_GUI)
	Set(TESTING_DEFAULT_TARGETS "${TESTING_DEFAULT_TARGETS};guftt")
Endif()
If(ENABLE_QT_GUI)
	Set(TESTING_DEFAULT_TARGETS "${TESTING_DEFAULT_TARGETS};cuftt")
Endif()

Function(Add_Test_Script TESTNAME TESTSCRIPT)
	Set(TESTTARGETS "${ARGV2}")
	If(NOT TESTTARGETS)
		Set(TESTTARGETS "uftt;guftt;cuftt")
	Endif()
	Foreach(TARGET ${TESTTARGETS})
		List(FIND TESTING_DEFAULT_TARGETS "${TARGET}" FOUND)
		If(NOT FOUND EQUAL -1)
			Add_Test(
				NAME "${TESTNAME}-${TARGET}"
				COMMAND "${TESTING_BASH}" ${TESTING_BASH_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/testdriver.sh" "${TESTSCRIPT}" "$<TARGET_FILE:${TARGET}>"
			)
		Endif()
	Endforeach()
EndFunction()

FIND_PACKAGE(MyBoost)

INCLUDE_DIRECTORIES("${BOOST_INCLUDE_DIR}")
LINK_DIRECTORIES("${BOOST_LIBRARY_DIR}")
IF(MSVC)
	# Disable autolinking from asio->regex because we don't use it
	ADD_DEFINITIONS("-DBOOST_REGEX_NO_LIB")
ENDIF()

Add_Test_Script(test-dummy1 dummy1.sh)

Add_Executable(test-dummy2 dummy2.cpp)
Add_Test(test-dummy2 test-dummy2)

Add_Executable(test-filesystem1 filesystem1.cpp)
Target_Link_Libraries(test-filesystem1 uftt-core lib-util ${BOOST_FILESYSTEM_LIBRARY} ${BOOST_SYSTEM_LIBRARY})
Add_Test(test-filesystem1 test-filesystem1)

Add_Test_Script(test-quit1 quit1.sh)

Add_Test_Script(test-download1 download1.sh)
