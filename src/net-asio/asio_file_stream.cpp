#include "asio_file_stream.h"
/*

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
	HANDLE whandle = ::CreateFile(
		TEXT(path.native_file_string().c_str()),
		((mode & in) ? GENERIC_READ : 0) | ((mode & out) ? GENERIC_WRITE : 0),
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL
	);
	if (whandle == INVALID_HANDLE_VALUE) {
		int error = GetLastError();
		if (error == 2 && (mode&create)) {
			whandle = ::CreateFile(
				TEXT(path.native_file_string().c_str()),
				((mode & in) ? GENERIC_READ : 0) | ((mode & out) ? GENERIC_WRITE : 0),
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				CREATE_NEW,
				FILE_FLAG_OVERLAPPED,
				NULL
			);
			if (whandle == INVALID_HANDLE_VALUE)
				error = GetLastError();
			else
				error = 0;
		}
		if (error != 0)
			throw boost::system::system_error(boost::system::error_code(error, boost::asio::error::get_system_category()));
	}
	//int x = GetLastError();
	asio_file_stream_base::assign(whandle);
}
#else // assume posix

#include <fcntl.h>
#include <aio.h>

void asio_file_stream::open(const boost::filesystem::path& path, unsigned int mode)
{
	int handle = ::open(path.native_file_string().c_str(),
		((mode&in)&&(mode&out))? O_RDWR : (((mode & in) ? O_RDONLY : 0) | ((mode & out) ? O_WRONLY : 0))
		| (mode&create)?O_CREAT:0
	);

	if (handle == -1) {
		std::cout << "open error: " << errno <<'\n';
	} else {
		std::cout << "success: " << handle << '\n';
	}

	asio_file_stream_base::assign(handle);
}
#endif
*/