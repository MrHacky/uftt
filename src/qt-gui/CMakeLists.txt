# CMake >= 2.4.0 is needed for Qt4
cmake_minimum_required(VERSION 2.4.0) 
find_package(Qt4 REQUIRED)
add_definitions("${QT_DEFINITIONS} -DQT_NO_KEYWORDS")
include(${QT_USE_FILE})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
IF(MINGW AND NOT QT_SHARED) 
	SET(QT_LIBRARIES "${QT_LIBRARIES}" Imm32 Winmm gdi32 comdlg32 oleaut32 winspool Ws2_32 ole32 uuid)
ENDIF(MINGW AND NOT QT_SHARED)


SET(UI_FILES
	MainWindow.ui
)

SET(UI_SRCS
	QTMain.cpp
	MainWindow.cpp
	QDragDropSignalWidget.cpp
)	

QT4_AUTOMOC(${UI_SRCS})

QT4_WRAP_UI(UI_H_FILES ${UI_FILES})

ADD_LIBRARY(uftt-qt
	${UI_SRCS}
	MainWindow.h
	QDragDropTreeWidget.h
	QDragDropSignalWidget.h
	QtLogger.h
	QtBooster.h
	QTMain.h
	${UI_FILES}
)

TARGET_LINK_LIBRARIES(uftt-qt
	uftt-lib-files
	${QT_LIBRARIES}
)
