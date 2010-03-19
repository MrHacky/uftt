STRING(COMPARE EQUAL "${ENABLE_QT_GUI}" "" AUTODETECT_QT_GUI)
IF(AUTODETECT_QT_GUI)
	find_package(Qt4)

	IF(QT4_FOUND)
		MESSAGE(STATUS "Enabling Qt gui")
		SET(ENABLE_QT_GUI ON CACHE BOOL "Enable building of the Qt gui")
	ELSE(QT4_FOUND)
		MESSAGE(STATUS "Disabling Qt gui")
		SET(ENABLE_QT_GUI OFF CACHE BOOL "Disable building of the Qt gui")
	ENDIF(QT4_FOUND)
ENDIF()

IF(ENABLE_QT_GUI)
	find_package(Qt4 REQUIRED)
	add_definitions("${QT_DEFINITIONS} -DQT_NO_KEYWORDS")
	include(${QT_USE_FILE})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	IF(MINGW AND NOT QT_SHARED)
		SET(QT_LIBRARIES "${QT_LIBRARIES}" Imm32 Winmm gdi32 comdlg32 oleaut32 winspool Ws2_32 ole32 uuid)
	ENDIF()

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
		DialogDirectoryChooser.cpp
		DialogPreferences.cpp
	)

	QT4_AUTOMOC(${UI_SRCS})

	QT4_WRAP_UI(UI_H_FILES ${UI_FILES})

	QT4_ADD_RESOURCES(QRC_SRCS Icons/icons.qrc)

	ADD_LIBRARY(uftt-qt
		${UI_SRCS}
		${QRC_SRCS}
		MainWindow.h
		QDragDropTreeWidget.h
		QDragDropSignalWidget.h
		QToggleHeaderAction.h
		QMarshaller.h
		DialogDirectoryChooser.h
		DialogPreferences.h
		QDebugStream.h
		QtLogger.h
		QTMain.h
		${UI_FILES}
	)

	TARGET_LINK_LIBRARIES(uftt-qt
		${QT_LIBRARIES}
		${QT_QTMAIN_LIBRARY}
	)
ENDIF(ENABLE_QT_GUI)
