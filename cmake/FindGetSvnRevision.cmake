Function(GetSvnRevision OUTPUTHEADER)
	Set(HELPERDIR "${PROJECT_SOURCE_DIR}/cmake/Helpers")
	IF(EXISTS "${PROJECT_SOURCE_DIR}/.svn/entries")
		ADD_CUSTOM_COMMAND(
			OUTPUT  "${OUTPUTHEADER}"
			COMMAND "${CMAKE_COMMAND}"
			ARGS
				-DINPUT="${PROJECT_SOURCE_DIR}/.svn/entries"
				-DOUTPUT="${OUTPUTHEADER}"
				-P "${HELPERDIR}/svnrev-svn.cmake"
			COMMENT "SvnRevision (svn)"
			DEPENDS "${PROJECT_SOURCE_DIR}/.svn/entries"
		)
	ELSEIF(EXISTS "${PROJECT_SOURCE_DIR}/.git/svn/git-svn/unhandled.log")
		ADD_CUSTOM_COMMAND(
			OUTPUT  "${OUTPUTHEADER}"
			COMMAND "${CMAKE_COMMAND}"
			ARGS
				-DINPUT="${PROJECT_SOURCE_DIR}/.git/svn/git-svn/unhandled.log"
				-DOUTPUT="${OUTPUTHEADER}"
				-P "${HELPERDIR}/svnrev-git.cmake"
			COMMENT "SvnRevision (git)"
			DEPENDS "${PROJECT_SOURCE_DIR}/.git/svn/git-svn/unhandled.log"
		)
	ELSE()
		ADD_CUSTOM_COMMAND(
			OUTPUT  "${OUTPUTHEADER}"
			COMMAND "${CMAKE_COMMAND}"
			ARGS
				-DOUTPUT="${OUTPUTHEADER}"
				-P "${HELPERDIR}/svnrev-dummy.cmake"
			COMMENT "SvnRevision (dummy)"
		)
	ENDIF()
EndFunction()
