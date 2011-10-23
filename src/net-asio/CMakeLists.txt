ADD_LIBRARY(lib-net-asio
	asio_file_stream.h
	#asio_file_stream.cpp
	asio_dgram_conn.h
	asio_http_request.h

	ipaddr_watcher.h
	ipaddr_watcher.cpp
	ipaddr_watcher_osimpl.cpp
)

#TARGET_LINK_LIBRARIES(lib-net-asio
#)
