ADD_LIBRARY(uftt-lib-network
	NetworkThread.cpp
	NetworkThread.h
	SharedData.cpp
	SharedData.h
	CrossPlatform.h
)

TARGET_LINK_LIBRARIES(uftt-lib-network
	boost_thread${BOOST_LIB_SUFFIX}
)