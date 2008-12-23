SET(NETMODULES
	SimpleBackend
	HTTPBackend
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

SET(LINK_INC_FILE ${CMAKE_CURRENT_BINARY_DIR}/NetModuleLinker.inc)
FILE(WRITE "${LINK_INC_FILE}" "\n")

FOREACH(netmod ${NETMODULES})
	FILE(APPEND "${LINK_INC_FILE}" "LINK_NETMODULE_CLASS(${netmod});\n")
	SET(NETMODULE_SOURCE_FILES ${NETMODULE_SOURCE_FILES}
		${netmod}.h
		${netmod}.cpp
	)
ENDFOREACH(netmod)

ADD_LIBRARY(uftt-network
	INetModule.h
	SimpleConnection.h
	SimpleTCPConnection.h
	UDPSemiConnection.h
	ConnectionBase.h          ConnectionBase.cpp
	NetModuleLinker.h         NetModuleLinker.cpp
	Misc.h                    Misc.cpp
	${NETMODULE_SOURCE_FILES} ${LINK_INC_FILE}
)
