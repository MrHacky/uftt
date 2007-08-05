ADD_LIBRARY(uftt-lib-files
	FileInfo.cpp
	FileInfo.h
)

TARGET_LINK_LIBRARIES(uftt-lib-files
	uftt-lib-sha1
	boost_filesystem${BOOST_LIB_SUFFIX}
)