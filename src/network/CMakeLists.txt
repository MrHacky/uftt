ADD_LIBRARY(uftt-lib-network
	NetworkThread.cpp
	NetworkThread.h
	CrossPlatform.h
)

TARGET_LINK_LIBRARIES(uftt-lib-network
	boost_thread${BOOST_LIB_SUFFIX}
)