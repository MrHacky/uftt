#ifndef ASIO_FILE_STREAM
#define ASIO_FILE_STREAM

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#ifdef WIN32
typedef boost::asio::windows::stream_handle asio_file_stream_base;
#else
typedef boost::asio::posix::stream_descriptor asio_file_stream_base;
#endif

class asio_file_stream: public asio_file_stream_base {
	private:
		void assign(const native_type & native_descriptor);
		boost::system::error_code assign(const native_type & native_descriptor,	boost::system::error_code & ec);

	public:
		enum openmode {
			in     = 1 << 0,
			out    = 1 << 1,
			app    = 1 << 2,
			ate    = 1 << 3,
			binary = 1 << 4,
			trunc  = 1 << 5,
		};

		asio_file_stream(boost::asio::io_service & io_service);

		asio_file_stream(boost::asio::io_service & io_service, const boost::filesystem::path& path, unsigned int mode = in|out);

		void open(const boost::filesystem::path& path, unsigned int mode = in|out);
};

#endif//ASIO_FILE_STREAM

