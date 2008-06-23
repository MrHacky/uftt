#include "asio_file_stream.h"


asio_file_stream::asio_file_stream(boost::asio::io_service & io_service)
: asio_file_stream_base(io_service)
{
}

asio_file_stream::asio_file_stream(boost::asio::io_service & io_service, const boost::filesystem::path& path, unsigned int mode)
: asio_file_stream_base(io_service)
{
	open(path, mode);
}

#ifdef WIN32
void asio_file_stream::open(const boost::filesystem::path& path, unsigned int mode)
{
	native_type handle;
	HANDLE whandle;
	asio_file_stream_base::assign(whandle);
}
#else // assume posix
void asio_file_stream::open(const boost::filesystem::path& path, unsigned int mode)
{
}
#endif
