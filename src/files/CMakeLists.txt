ADD_LIBRARY(uftt-lib-files
	FileInfo.cpp
	FileInfo.h
)

TARGET_LINK_LIBRARIES(uftt-lib-files
	boost_filesystem-gcc41
)