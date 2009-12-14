#ifndef ASIO_FILE_STREAM
#define ASIO_FILE_STREAM

#include "../Types.h"
#include "../Platform.h"

//#define BOOST_ASIO_DISABLE_EPOLL
//#define BOOST_ASIO_DISABLE_IOCP

// offsets are wrong atm, use thread queue implementation
#define DISABLE_DISKIO_WIN32_IOCP_HANDLE

#if defined(WIN32) && !defined(BOOST_ASIO_DISABLE_IOCP) && !defined(DISABLE_DISKIO_WIN32_IOCP_HANDLE)
#else
#define DISABLE_DISKIO_WIN32_IOCP_HANDLE
#endif

/*
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace openwrapper {
	enum options {
		o_rdonly = O_RDONLY,
		o_wronly = O_WRONLY,
		o_rdwr = O_RDWR,

		o_append = O_APPEND,
		o_creat = O_CREAT,
		o_create = O_CREAT,
		o_excl = O_EXCL,
		o_trunc = O_TRUNC,
		o_binary = O_BINARY,
		o_text = O_TEXT,

		s_iread = S_IREAD,    // linux S_IROTH|S_IRUSR|S_IRGRP
		s_iwrite = S_IWRITE,  // linux S_IWOTH|S_IWUSR|S_IWGRP
		s_iexec = 0,          // linux S_IXOTH|S_IXUSR|S_IXGRP
	};
}
*/
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include "../util/PosixError.h"

/*****************************************************************/
// Service default implementation?

namespace services {
	class diskio_filetype;

	class diskio_service //: public boost::asio::detail::service_base<diskio_queue_service>
	{
		private:
			boost::asio::io_service& io_service_;
			boost::asio::io_service work_io_service;
			boost::thread work_thread;

			void thread_loop() {
				boost::asio::io_service::work work(work_io_service);
				work_io_service.run();
			}

		public:
			explicit diskio_service(boost::asio::io_service& io_service)
				: io_service_(io_service)
			{
				work_thread = boost::thread(boost::bind(&diskio_service::thread_loop, this)).move();
			}

			~diskio_service()
			{
				stop();
			}

			typedef diskio_filetype filetype;

			boost::asio::io_service& get_io_service()
			{
				return io_service_;
			}

			boost::asio::io_service& get_work_service()
			{
				return work_io_service;
			}

			void stop()
			{
				work_io_service.stop();
				work_thread.join();
			}

			template <typename Path, typename Handler>
			void async_create_directory(const Path& path, const Handler& handler)
			{
				work_io_service.dispatch(
					helper_create_directory<Path, Handler>(io_service_, path, handler)
				);
			}

			template <typename Path, typename Handler>
			void async_open_file(const Path& path, int flags, filetype& file, const Handler& handler)
			{
				work_io_service.dispatch(
					helper_open_file<Path, Handler>(io_service_, path, flags, file, handler)
				);
			}

		private:
			template <typename Path, typename Handler>
			struct helper_open_file {
				boost::asio::io_service& service;
				Path path;
				int flags;
				filetype& file;
				Handler handler;

				helper_open_file(boost::asio::io_service& service_, const Path& path_, int flags_, filetype& file_, const Handler& handler_)
					: service(service_), path(path_), flags(flags_), file(file_), handler(handler_)
				{
				}

				void operator()();
			};

			template <typename Path, typename Handler>
			struct helper_create_directory {
				boost::asio::io_service& service;
				Path path;
				Handler handler;

				helper_create_directory(boost::asio::io_service& service_, const Path& path_, const Handler& handler_)
					: service(service_), path(path_), handler(handler_)
				{
				}

				void operator()() {
					boost::system::error_code res;
					try {
						boost::filesystem::create_directory(path);
					} catch (boost::filesystem::basic_filesystem_error<Path>& e) {
						res = e.code();
					}
					service.dispatch(boost::bind(handler, res));
				}
			};

	};

#ifndef DISABLE_DISKIO_WIN32_IOCP_HANDLE

	class diskio_filetype: public boost::asio::windows::stream_handle {
		private:
			void assign(const native_type & native_descriptor);
			boost::system::error_code assign(const native_type & native_descriptor,	boost::system::error_code & ec);

		public:
			enum openmode {
				in     = 1 << 0,
				out    = 1 << 1,
				app    = 1 << 2,
				ate    = 1 << 3,
				text   = 1 << 4,
				trunc  = 1 << 5,
				create = 1 << 6,
			};

			diskio_filetype(diskio_service& service)
				: boost::asio::windows::stream_handle(service.get_io_service())
			{
			}

			boost::system::error_code open(const boost::filesystem::path& path, unsigned int mode = in|out)
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
						return boost::system::error_code(error, boost::asio::error::get_system_category());
				}
				boost::asio::windows::stream_handle::assign(whandle);
				return  boost::system::error_code();
			}
	};

#else
	class diskio_filetype {
		private:
			diskio_service& service;
			FILE* fd;
			uint32 testlen;

		public:
			enum openmode {
				in     = 1 << 0,
				out    = 1 << 1,
				app    = 1 << 2,
				ate    = 1 << 3,
				text   = 1 << 4,
				trunc  = 1 << 5,
				create = 1 << 6,
			};

			diskio_filetype(diskio_service& service_)
				: service(service_)
			{
				fd = NULL;
				testlen = 716244992;//683*1024*1024;
			}

			boost::asio::io_service& get_io_service()
			{
				return service.get_io_service();
			}

			boost::system::error_code open(const boost::filesystem::path& path, unsigned int mode = in|out)
			{
				std::string openmode = "";
				if (mode&out && mode&create) openmode += "w";
				else if (mode&in) openmode += "r";
				if (mode&in && mode&out) openmode += "+";
				if (!(mode&text)) openmode += "b";
				fd = fopen(path.native_file_string().c_str(), openmode.c_str());
				if (fd != NULL)
					return boost::system::error_code();
				return ext::posix_error::make_error_code(errno);
			}

			void close() {
				if (fd != NULL) {
					fclose(fd);
					fd = NULL;
				}
			}

			template <typename MBS, typename Handler>
			void async_read_some(MBS mbs, const Handler& handler)
			{
				service.get_work_service().dispatch(
					helper_read_some<MBS, Handler>(service.get_io_service(), mbs, fd, handler, testlen)
				);
			}

			template <typename CBS, typename Handler>
			void async_write_some(CBS& cbs, const Handler& handler)
			{
				service.get_work_service().dispatch(
					helper_write_some<CBS, Handler>(service.get_io_service(), cbs, fd, handler)
				);
			}

			void fseeka(uint64 offset) {
				platform::fseek64a(fd, offset);
			}

		private:
			template <typename CBS, typename Handler>
			struct helper_write_some {
				boost::asio::io_service& service;
				CBS cbs;
				FILE* fd;
				Handler handler;

				helper_write_some(boost::asio::io_service& service_, CBS& cbs_, FILE* fd_, const Handler& handler_)
					: service(service_), cbs(cbs_), fd(fd_), handler(handler_)
				{
				}

				void operator()() {
					if (fd == NULL) {
						service.dispatch(boost::bind<void>(handler,
							boost::system::error_code(boost::asio::error::bad_descriptor),
							0)
						);
						return;
					}
					if (int error = ferror(fd)) {
						service.dispatch(boost::bind<void>(handler,
							ext::posix_error::make_error_code(error),
							0)
						);
						return;
					}
					const void* buf = NULL;
					size_t buflen = 0;
					typename CBS::const_iterator iter = cbs.begin();
					typename CBS::const_iterator end = cbs.end();
					for (;buflen == 0 && iter != end; ++iter) {
						// at least 1 buffer
						boost::asio::const_buffer buffer(*iter);
						buf = boost::asio::buffer_cast<const void*>(buffer);
						buflen = boost::asio::buffer_size(buffer);
					}
					if (buflen == 0) {
						// no-op
						service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
						return;
					};
					//> DEBUG HAX
					/*
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), buflen));
					return;
					*/
					//< DEBUG HAX

					size_t written = fwrite(buf, 1, buflen, fd);

					if (written == 0) {
						service.dispatch(boost::bind<void>(handler,
							ext::posix_error::make_error_code(ferror(fd)),
							0)
						);
						return;
					}
					if (written < buflen) {
						// this is an error, but we ignore it and hope the next write will return 0
						// this is so we can report this partial success to the handler
					}
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), written));
				}
			};

			template <typename MBS, typename Handler>
			struct helper_read_some {
				boost::asio::io_service& service;
				MBS mbs;
				FILE* fd;
				Handler handler;
				uint32& testlen;

				helper_read_some(boost::asio::io_service& service_, MBS& mbs_, FILE* fd_, const Handler& handler_, uint32& testlen_)
					: service(service_), mbs(mbs_), fd(fd_), handler(handler_), testlen(testlen_)
				{
				}

				void operator()() {
					if (fd == NULL) {
						service.dispatch(boost::bind<void>(handler,
							boost::system::error_code(boost::asio::error::bad_descriptor),
							0)
						);
						return;
					}
					if (int error = ferror(fd)) {
						service.dispatch(boost::bind<void>(handler,
							ext::posix_error::make_error_code(error),
							0)
						);
						return;
					}
					void* buf;
					size_t buflen = 0;
					typename MBS::const_iterator iter = mbs.begin();
					typename MBS::const_iterator end = mbs.end();
					for (;buflen == 0 && iter != end; ++iter) {
						// at least 1 buffer
						boost::asio::mutable_buffer buffer(*iter);
						buf = boost::asio::buffer_cast<void*>(buffer);
						buflen = boost::asio::buffer_size(buffer);
					}
					if (buflen == 0) {
						// no-op
						service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
						return;
					};

					//> DEBUG HAX
					/*
					size_t hxread = buflen;
					if (hxread > testlen) hxread = testlen;
					testlen -= hxread;
					if (hxread > 0)
						service.dispatch(boost::bind<void>(handler, boost::system::error_code(), hxread));
					else
						service.dispatch(boost::bind<void>(handler, boost::system::error_code(-1,boost::asio::error::get_system_category()), 0));
					return;
					*/
					//< DEBUG HAX

					size_t read = fread(buf, 1, buflen, fd);

					if (read == 0) {
						if (feof(fd)) {
							service.dispatch(boost::bind<void>(handler,
								boost::asio::error::eof,
								0)
							);
						} else {
							service.dispatch(boost::bind<void>(handler,
								ext::posix_error::make_error_code(ferror(fd)),
								0)
							);
						}
						return;
					}
					if (read < buflen) {
						// this is an error, but we ignore it and hope the next read will return 0
						// this is so we can report this partial success to the handler
					}
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), read));
				}
			};
	};
#endif

	template <typename Path, typename Handler>
	inline void diskio_service::helper_open_file<Path, Handler>::operator()()
	{
		boost::system::error_code res;
		res = file.open(path, flags);
		service.dispatch(boost::bind(handler, res));
	}

} // namespace services



#endif//ASIO_FILE_STREAM

