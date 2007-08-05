cmake_minimum_required(VERSION 2.4.0) # needed for Qt4
find_package(Qt4 REQUIRED) # find and setup Qt4 for this project

include(${QT_USE_FILE})

SET(UI_FILES
	MainWindow.ui
)

QT4_WRAP_UI(UI_H_FILES ${UI_FILES})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

ADD_EXECUTABLE(uftt-qt
	Main.cpp
	MainWindow.cpp
	MainWindow.h
	QSharesTreeWidget.cpp
	QSharesTreeWidget.h
	${UI_FILES}
)

TARGET_LINK_LIBRARIES(uftt-qt ${QT_LIBRARIES})
