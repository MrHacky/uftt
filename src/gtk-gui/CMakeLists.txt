STRING(COMPARE EQUAL "${ENABLE_GTK_GUI}" "" AUTODETECT_GTK_GUI)
IF(AUTODETECT_GTK_GUI)
	find_package(PkgConfig)

	IF(PKG_CONFIG_FOUND)
		pkg_check_modules(GTKMM24 REQUIRED gtkmm-2.4>=2.18.3)
		pkg_check_modules(GTHREAD REQUIRED gthread-2.0)
		
		IF(GTKMM24_FOUND)
			IF(GTHREAD_FOUND)
				MESSAGE(STATUS "Enabling gtk-gui")
				SET(ENABLE_GTK_GUI ON CACHE BOOL "Enable building of the gtk gui")
				IF(WIN32)
# FIXME: The vsprops parsing magic should handle this now. Check to see if
#         the whole pkg_check_modules still works / is needed on win32
#
#					               #1234567890          1234567890          1234567890          1234567890
#					               #          1234567890          1234567890          1234567890          1234567890
#					MESSAGE(STATUS "NOTE: You will still need to manually set GTKMM24_INCLUDE_DIRS and")
#					MESSAGE(STATUS "GTHREAD_INCLUDE_DIRS, since pkg-config is broken on WIN32: it will A) generate")
#					MESSAGE(STATUS "CFLAGS only for gnu-tools; and B) can not handle path's with spaces in them")
#					MESSAGE(STATUS "(e.g. gtkmm's default installation path ``c:\\progam files\\gtkmm\\'' will cause")
#					MESSAGE(STATUS "build failures due to bogus include directives like ``-IC:/Program'').")
#					MESSAGE(STATUS "If you are using MSVC there are some Property Sheets in gtkmm's ``MSVC''")
#					MESSAGE(STATUS "directory that you may find usefull.")
				ENDIF(WIN32)
			ELSE(GTHREAD_FOUND)
				MESSAGE(STATUS "Missing GThread, disabling gtk-gui")
				SET(ENABLE_GTK_GUI OFF CACHE BOOL "Enable building of the gtk gui")
			ENDIF(GTHREAD_FOUND)
		ELSE(GTKMM24_FOUND)
			MESSAGE(STATUS "Missing Gtkmm, disabling gtk-gui")
			SET(ENABLE_GTK_GUI OFF CACHE BOOL "Enable building of the gtk gui")
		ENDIF(GTKMM24_FOUND)
	ELSE(PKG_CONFIG_FOUND)
		MESSAGE(STATUS "Missing PkgConfig, cannot search for gtkmm and gthread, disabling gtk-gui")
		SET(ENABLE_GTK_GUI OFF CACHE BOOL "Enable building of the gtk gui")
	ENDIF(PKG_CONFIG_FOUND)
ENDIF(AUTODETECT_GTK_GUI)

SET(GENERATE_UFTT_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK "" CACHE FILEPATH "Work around bugs in ImageMagick's SVG module")

IF(ENABLE_GTK_GUI)
	find_package(PkgConfig REQUIRED)
	find_package(ImageMagick COMPONENTS convert)
	find_package(SearchUtils REQUIRED)
	GLOB_PATHS(GLOB_PATHS NOBASERESULT GLOBS "*" BASES "${CMAKE_EXTRA_LIBS_PATH}")
	FOREACH(GLOB_PATH ${GLOB_PATHS})
		IF(NOT PNG2ICO_EXECUTABLE)
			find_program(PNG2ICO_EXECUTABLE NAMES png2ico png2ico.exe PATHS "${GLOB_PATH}")
		ENDIF(NOT PNG2ICO_EXECUTABLE)
	ENDFOREACH(GLOB_PATH)
	pkg_check_modules(GTKMM24 gtkmm-2.4 REQUIRED)
	pkg_check_modules(GTHREAD gthread-2.0 REQUIRED)

#	MESSAGE(STATUS "GTKMM24_INCLUDE_DIRS: ${GTKMM24_INCLUDE_DIRS}")
#	MESSAGE(STATUS "GTHREAD_INCLUDE_DIRS: ${GTHREAD_INCLUDE_DIRS}")


	IF(MSVC)
		IF(MSVC80)
			find_file(GTKMM_PROPERTY_SHEET       "gtkmm-vc80-2_4.vsprops"   "$ENV{GTKMM_BASEPATH}\\MSVC\\")
			find_file(GTKMM_DEBUG_PROPERTY_SHEET "gtkmm-vc80-d-2_4.vsprops" "$ENV{GTKMM_BASEPATH}\\MSVC\\")
		ELSE(MSVC80)
		IF(MSVC90)
			find_file(GTKMM_PROPERTY_SHEET       "gtkmm-vc90-2_4.vsprops"   "$ENV{GTKMM_BASEPATH}\\MSVC\\")
			find_file(GTKMM_DEBUG_PROPERTY_SHEET "gtkmm-vc90-d-2_4.vsprops" "$ENV{GTKMM_BASEPATH}\\MSVC\\")
		ELSE(MSVC90)
			find_file(GTKMM_PROPERTY_SHEET       "gtkmm-2_4.vsprops"        "$ENV{GTKMM_BASEPATH}\\MSVC\\")
			find_file(GTKMM_DEBUG_PROPERTY_SHEET "gtkmm-d-2_4.vsprops"      "$ENV{GTKMM_BASEPATH}\\MSVC\\")
		ENDIF(MSVC90)
		ENDIF(MSVC80)
		IF(CMAKE_BUILD_TYPE MATCHES Debug)
			SET(_PROPERTY_SHEET ${GTKMM_DEBUG_PROPERTY_SHEET})
		ELSE(CMAKE_BUILD_TYPE MATCHES Debug)
			SET(_PROPERTY_SHEET ${GTKMM_PROPERTY_SHEET})
		ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

		file(STRINGS "${_PROPERTY_SHEET}" GTKMM_ADDITIONAL_INCLUDE_DIRECTORIES REGEX AdditionalIncludeDirectories)
		file(STRINGS "${_PROPERTY_SHEET}" GTKMM_ADDITIONAL_DEPENDENCIES REGEX AdditionalDependencies)
		file(STRINGS "${_PROPERTY_SHEET}" GTKMM_ADDITIONAL_LIBRARY_DIRECTORIES REGEX AdditionalLibraryDirectories)
		STRING(REGEX REPLACE "\\\\" "\\\\\\\\" GTKMM_BASEPATH_REGEX $ENV{GTKMM_BASEPATH})
		FOREACH(S GTKMM_ADDITIONAL_INCLUDE_DIRECTORIES GTKMM_ADDITIONAL_DEPENDENCIES GTKMM_ADDITIONAL_LIBRARY_DIRECTORIES)
			STRING(REGEX REPLACE ^.*AdditionalIncludeDirectories[^=]*=[^\"]*\"   ""                        "${S}" "${${S}}")
			STRING(REGEX REPLACE ^.*AdditionalDependencies[^=]*=[^\"]*\"         ""                        "${S}" "${${S}}")
			STRING(REGEX REPLACE ^.*AdditionalLibraryDirectories[^=]*=[^\"]*\"   ""                        "${S}" "${${S}}")
			STRING(REGEX REPLACE \"[^\"]*$                                       ""                        "${S}" "${${S}}")
			STRING(REGEX REPLACE "\\$\\(GTKMM_BASEPATH\\)"                       ${GTKMM_BASEPATH_REGEX}   "${S}" "${${S}}")
			STRING(REGEX REPLACE "\\\\;"                                         ";"                       "${S}" "${${S}}")
			STRING(REGEX REPLACE "&quot\\;"                                      ""                        "${S}" "${${S}}")
		ENDFOREACH(S)
		SET(GTKMM24_INCLUDE_DIRS "${GTKMM_ADDITIONAL_INCLUDE_DIRECTORIES}")
		SET(GTKMM24_LIBRARIES    "${GTKMM_ADDITIONAL_DEPENDENCIES}")
		SET(GTKMM24_LIBRARY_DIRS "${GTKMM_ADDITIONAL_LIBRARY_DIRECTORIES}")
	ENDIF(MSVC)

	INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIR}
	                    ${GTKMM24_INCLUDE_DIRS}
	                    ${GTHREAD_INCLUDE_DIRS}
	                    ${CMAKE_CURRENT_SOURCE_DIR} # For boost/preprocessor/iteration
	                   )
	LINK_DIRECTORIES   (${BOOST_LIBRARY_DIR}
	                    ${GTKMM24_LIBRARY_DIRS}
	                    ${GTHREAD_LIBRARY_DIRS}
	                   )
	LINK_LIBRARIES     (${BOOST_LIBRARIES}
	                    ${GTKMM24_LIBRARIES}
	                    ${GTHREAD_LIBRARIES}
	                   )
	
	MACRO(GENERATE_ICON_IMAGE SIZE BPP COLOR_COUNT)
		STRING(COMPARE NOTEQUAL "X${GENERATE_UFTT_GTK_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK}X" "XX" _GENERATE_UFTT_GTK_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK)
		IF(_GENERATE_UFTT_GTK_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK)
			SET(_UFTT_GTK_ICON_NAME "${CMAKE_CURRENT_BINARY_DIR}/uftt_icon_${SIZE}_${BPP}_${COLOR_COUNT}.png")
		ELSE(_GENERATE_UFTT_GTK_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK)
			SET(_UFTT_GTK_ICON_NAME "${CMAKE_CURRENT_BINARY_DIR}/uftt_icon_${SIZE}_${BPP}_${COLOR_COUNT}.svg")
		ENDIF(_GENERATE_UFTT_GTK_ICON_FROM_SOURCE_PNG_INSTEAD_OF_SVG_BECAUSE_OF_B0RKEN_SVG_SUPPORT_IN_IMAGEMAGICK)
		ADD_CUSTOM_COMMAND(OUTPUT "${_UFTT_GTK_ICON_NAME}" PRE_BUILD
					   COMMAND "${IMAGEMAGICK_CONVERT_EXECUTABLE}" -resize "${SIZE}"
										       -depth "${BPP}"
										       -colors "${COLOR_COUNT}"
										       "${CMAKE_CURRENT_SOURCE_DIR}/uftt_icon.png"
										       "${_UFTT_GTK_ICON_NAME}"
					   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/uftt_icon.svg"
					  )
		SET(UFTT_GTK_ICON_DEPS ${UFTT_GTK_ICON_DEPS} "${_UFTT_GTK_ICON_NAME}")
	ENDMACRO(GENERATE_ICON_IMAGE)
	
	UNSET(WIN32_UFTT_GTK_ICON_RESOURCES)
	UNSET(UFTT_GTK_ICON_RESOURCE_FILE)
	IF(WIN32)
		UNSET(UFTT_GTK_ICON_DEPS)
		FOREACH(COLOR_COUNT 0 256 16 65535) # NOTE: -colors 0 == do not use color reduction
			FOREACH(BPP 32 16 8)
				FOREACH(RESOLUTION 32x32 16x16 24x24 48x48)
					GENERATE_ICON_IMAGE(${RESOLUTION} ${BPP} ${COLOR_COUNT})
				ENDFOREACH(RESOLUTION)
			ENDFOREACH(BPP)
		ENDFOREACH(COLOR_COUNT)
		
		ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/uftt.ico" PREBUILD
		                   COMMAND "${PNG2ICO_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/uftt.ico"                                                  ${UFTT_GTK_ICON_DEPS}
		                   DEPENDS ${UFTT_GTK_ICON_DEPS}
		                  )
		IF(PNG2ICO_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
			SET(WIN32_UFTT_GTK_ICON_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/uftt.ico" "${CMAKE_CURRENT_SOURCE_DIR}/uftt_icon.rc")
		ELSE(PNG2ICO_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
			MESSAGE(STATUS "NOTE: In order to build the icons PNG2ICO_EXECUTABLE and IMAGEMAGICK_CONVERT_EXECUTABLE need to be set")
		ENDIF(PNG2ICO_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
	ENDIF(WIN32)
	
	UNSET(GENERATED_BIN_HEADERS)
	MACRO(EMBED_BINARY BINPATH BINFILE)
		SET(_EMBED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/${BINPATH}/${BINFILE}")
		SET(_EMBED_DST "${CMAKE_CURRENT_BINARY_DIR}/${BINFILE}.h")
		SET(_EMBED_BIN "${CMAKE_BINARY_DIR}/src/util/bin2c")
		ADD_CUSTOM_COMMAND(
			OUTPUT  "${_EMBED_DST}" PRE_BUILD
			COMMAND "${_EMBED_BIN}" -c
			        "${_EMBED_SRC}"
			        "${_EMBED_DST}"
			DEPENDS "${_EMBED_BIN}"
			        "${_EMBED_SRC}"
		)
		SET(GENERATED_BIN_HEADERS ${GENERATED_BIN_HEADERS} "${_EMBED_DST}")
	ENDMACRO(EMBED_BINARY)
	
	EMBED_BINARY("../qt-gui/Icons/" "uftt-16x16.png")
#	EMBED_BINARY("../qt-gui/Icons/" "uftt-22x22.png") # FIXME: Does not exist yet
	EMBED_BINARY("../qt-gui/Icons/" "uftt-32x32.png")
	EMBED_BINARY("../qt-gui/Icons/" "uftt-48x48.png")
	
	# make sure embedded file's headers can be found
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
	ADD_LIBRARY(uftt-gtk
	            ${WIN32_UFTT_GTK_ICON_RESOURCES}
	            ${GENERATED_BIN_HEADERS}
	            GTKMain.h              	GTKMain.cpp
	            GTKImpl.cpp            	GTKImpl.h
	            AutoScrollingWindow.cpp	AutoScrollingWindow.h
	            ShareList.cpp          	ShareList.h
	            TaskList.cpp           	TaskList.h
	            ShowURI.cpp            	ShowURI.h
	            PreferencesDialog.cpp  	PreferencesDialog.h
	            dispatcher_marshaller.h
	            OStreamGtkTextBuffer.h
	)

	TARGET_LINK_LIBRARIES(uftt-gtk

	)

	
	IF(MSVC)
		file(STRINGS "${_PROPERTY_SHEET}" GTKMM_DISABLE_SPECIFIC_WARNINGS REGEX DisableSpecificWarnings)
		STRING(REGEX REPLACE ^.*DisableSpecificWarnings[^=]*=[^\"]*\" ";"   "GTKMM_DISABLE_SPECIFIC_WARNINGS" "${GTKMM_DISABLE_SPECIFIC_WARNINGS}")
		STRING(REGEX REPLACE \"[^\"]*$                                ""    "GTKMM_DISABLE_SPECIFIC_WARNINGS" "${GTKMM_DISABLE_SPECIFIC_WARNINGS}")
		STRING(REGEX REPLACE "\\\\;"                                  ";"   "GTKMM_DISABLE_SPECIFIC_WARNINGS" "${GTKMM_DISABLE_SPECIFIC_WARNINGS}")
		STRING(REGEX REPLACE "\\;"                                    " /wd" "GTKMM_DISABLE_SPECIFIC_WARNINGS" "${GTKMM_DISABLE_SPECIFIC_WARNINGS}")
		SET_TARGET_PROPERTIES(uftt-gtk PROPERTIES COMPILE_FLAGS "${GTKMM_DISABLE_SPECIFIC_WARNINGS}")
	ENDIF(MSVC)

ENDIF(ENABLE_GTK_GUI)
