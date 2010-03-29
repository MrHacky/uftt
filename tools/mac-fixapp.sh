#!/bin/sh

QT_FW_PATH="/Library/Frameworks"
QT_VSTRING="Versions/4"
ECHO=

${ECHO} mkdir -p uftt.app/Contents/Resources/
${ECHO} cp mac/src/qt-gui/Icons/uftt.icns uftt.app/Contents/Resources/

${ECHO} mkdir -p uftt.app/Contents/Frameworks/
for FW in QtGui QtCore; do
	${ECHO} cp -R ${QT_FW_PATH}/${FW}.framework uftt.app/Contents/Frameworks/
	${ECHO} rm uftt.app/Contents/Frameworks/${FW}.framework/Headers/*
	${ECHO} install_name_tool -id \
		@executable_path/../Frameworks/${FW}.framework/${QT_VSTRING}/${FW} \
		uftt.app/Contents/Frameworks/${FW}.framework/${QT_VSTRING}/${FW}
	for TGT in uftt.app/Contents/MacOS/uftt uftt.app/Contents/Frameworks/QtGui.framework/${QT_VSTRING}/QtGui; do
		${ECHO} install_name_tool -change \
			${FW}.framework/${QT_VSTRING}/${FW} \
			@executable_path/../Frameworks/${FW}.framework/${QT_VSTRING}/${FW} \
			${TGT}
	done
done
