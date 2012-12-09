
SET(ENABLE_CONSOLE_UI ON CACHE BOOL "Enable building of the console ui")

IF(ENABLE_CONSOLE_UI)
	ADD_LIBRARY(uftt-console
		cuftt.cpp
	)
ENDIF()
