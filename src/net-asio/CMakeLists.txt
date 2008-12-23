ADD_LIBRARY(lib-net-asio
	asio_file_stream.h
	#asio_file_stream.cpp
	asio_ipx.h
	asio_ipx.cpp
	asio_dgram_conn.h
	ipx_conn.h
	#ipx_conn.cpp
	asio_http_request.h
	ipaddr_watcher.h       ipaddr_watcher.cpp
)

#TARGET_LINK_LIBRARIES(lib-net-asio
#)
