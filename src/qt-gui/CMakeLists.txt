STRING(COMPARE EQUAL "${ENABLE_QT_GUI}" "" AUTODETECT_QT_GUI)
IF(AUTODETECT_QT_GUI OR ENABLE_QT_GUI)
	IF(NOT EXISTS "${QT_QMAKE_EXECUTABLE}")
		FIND_LIB_IN_EXTRALIBS(QT *qt* include bin)
		FIND_PROGRAM(QT_QMAKE_EXECUTABLE NAMES qmake PATHS ${QT_EXTRALIB_LIBRARY_PATHS} NO_DEFAULT_PATH)
	ENDIF()
ENDIF()
IF(AUTODETECT_QT_GUI)
	find_package(Qt5Core)

	IF(QT5_FOUND)
		MESSAGE(STATUS "Enabling Qt gui")
		SET(ENABLE_QT_GUI ON CACHE BOOL "Enable building of the Qt gui")
	ELSE(QT5_FOUND)
		MESSAGE(STATUS "Disabling Qt gui")
		SET(ENABLE_QT_GUI OFF CACHE BOOL "Disable building of the Qt gui")
	ENDIF(QT5_FOUND)
ENDIF()

IF(ENABLE_QT_GUI)
	Get_Filename_Component(QT_HINTS_DIR "${QT_QMAKE_EXECUTABLE}/../../lib/cmake/Qt5" ABSOLUTE)
	find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED HINTS "${QT_HINTS_DIR}")
	add_definitions("${QT_DEFINITIONS} -DQT_NO_KEYWORDS")
	include_directories(${CMAKE_CURRENT_BINARY_DIR})
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	IF(MINGW AND NOT QT_SHARED)
		SET(QT_LIBRARIES "${QT_LIBRARIES}" Imm32 Winmm gdi32 comdlg32 oleaut32 winspool Ws2_32 ole32 uuid)
	ENDIF()
	SET(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets)
	SET(QT_QTMAIN_LIBRARY "")

	SET(UI_FILES
		MainWindow.ui
		DialogPreferences.ui
		DialogDirectoryChooser.ui
	)

	SET(UI_SRCS
		QTMain.cpp
		QMarshaller.cpp
		MainWindow.cpp
		QDragDropSignalWidget.cpp
		QToggleHeaderAction.cpp
		QPathComboBox.cpp
		DialogDirectoryChooser.cpp
		DialogPreferences.cpp
		QExtPlatform.cpp
	)

	Set(CMAKE_AUTOMOC ON)

	QT5_WRAP_UI(UI_H_FILES ${UI_FILES})

	QT5_ADD_RESOURCES(QRC_SRCS Icons/icons.qrc)

	ADD_LIBRARY(uftt-qt
		${UI_SRCS}
		${QRC_SRCS}
		MainWindow.h
		QExtUTF8.h
		QDragDropTreeWidget.h
		QDragDropSignalWidget.h
		QToggleHeaderAction.h
		QPathComboBox.h
		QMarshaller.h
		DialogDirectoryChooser.h
		DialogPreferences.h
		QExtPlatform.h
		QDebugStream.h
		QTMain.h
		${UI_FILES}
	)

	TARGET_LINK_LIBRARIES(uftt-qt
		${QT_LIBRARIES}
		${QT_QTMAIN_LIBRARY}
	)
ENDIF(ENABLE_QT_GUI)
