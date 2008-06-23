ADD_LIBRARY(uftt-lib-network
	NetworkThread.cpp
	NetworkThread.h
	Packet.cpp
	Packet.h
	CrossPlatform.h
)

TARGET_LINK_LIBRARIES(uftt-lib-network
	${BOOST_THREAD_LIBRARY}
)