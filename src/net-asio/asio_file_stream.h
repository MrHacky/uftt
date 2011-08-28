#ifndef ASIO_FILE_STREAM
#define ASIO_FILE_STREAM

#include "../Types.h"
#include "../Platform.h"

//#define BOOST_ASIO_DISABLE_EPOLL
//#define BOOST_ASIO_DISABLE_IOCP

// offsets are wrong atm, use thread queue implementation
//#define DISABLE_DISKIO_WIN32_IOCP_HANDLE

#if !defined(WIN32) || defined(BOOST_ASIO_DISABLE_IOCP) || defined(DISABLE_DISKIO_WIN32_IOCP_HANDLE)
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

namespace ext { namespace asio {
	//class fstream;

	template <typename File>
	class fstream_service: public boost::asio::io_service::service
	{
		public:
			static boost::asio::io_service::id id;

			explicit fstream_service(boost::asio::io_service &io_service)
			: boost::asio::io_service::service(io_service)
			, worker_work(worker_service)
			, worker_thread(boost::bind(&boost::asio::io_service::run, &worker_service))
			{
			}

			template <typename Handler>
			void async_open(File* file, const ext::filesystem::path& path, int flags, const Handler& handler)
			{
				worker_service.post(helper_open_file<Handler>(get_io_service(), file, path, flags, handler));
			}

			boost::asio::io_service& get_work_service()
			{
				return worker_service;
			}

		private:
			void shutdown_service()
			{
				worker_service.stop();
				worker_thread.join();
			}

			template <typename Handler>
			struct helper_open_file {
				boost::asio::io_service& service;
				ext::filesystem::path path;
				int flags;
				File* file;
				Handler handler;

				helper_open_file(boost::asio::io_service& service_, File* file_, const ext::filesystem::path& path_, int flags_, const Handler& handler_)
					: service(service_), path(path_), flags(flags_), file(file_), handler(handler_)
				{
				}

				void operator()()
				{
					boost::system::error_code res;
					file->open(path, flags, res);
					service.post(boost::bind(handler, res));
				}
			};

			boost::asio::io_service worker_service;
			boost::asio::io_service::work worker_work;
			boost::thread worker_thread;
	};

	template <typename File>
	boost::asio::io_service::id fstream_service<File>::id;

#ifndef DISABLE_DISKIO_WIN32_IOCP_HANDLE

	class fstream: public boost::asio::windows::random_access_handle {
		private:
			boost::uint64_t offset;
			bool written;

			static HANDLE openhandle(const std::string& path, DWORD access, DWORD creation)
			{
				return ::CreateFileA(
					path.c_str(),
					access,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					creation,
					FILE_FLAG_OVERLAPPED,
					NULL
				);
			}
			static HANDLE openhandle(const std::wstring& path, DWORD access, DWORD creation)
			{
				return ::CreateFileW(
					path.c_str(),
					access,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					creation,
					FILE_FLAG_OVERLAPPED,
					NULL
				);
			}

			ext::asio::fstream_service<fstream>& get_fstream_service()
			{
				return boost::asio::use_service<ext::asio::fstream_service<fstream> >(this->get_io_service());
			}

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

			fstream(boost::asio::io_service& service)
				: boost::asio::windows::random_access_handle(service)
				, offset(0), written(false)
			{
			}

			template <typename Handler>
			void async_open(const ext::filesystem::path& path, int flags, const Handler& handler)
			{
				get_fstream_service().async_open(this, path, flags, handler);
			}

			void open(const ext::filesystem::path& path, unsigned int mode, boost::system::error_code& err)
			{
				HANDLE whandle = openhandle(
					path.external_file_string(),
					((mode & in) ? GENERIC_READ : 0) | ((mode & out) ? GENERIC_WRITE : 0),
					OPEN_EXISTING
				);
				if (whandle == INVALID_HANDLE_VALUE) {
					int error = GetLastError();
					if (error == 2 && (mode&create)) {
						whandle = openhandle(
							path.external_file_string(),
							((mode & in) ? GENERIC_READ : 0) | ((mode & out) ? GENERIC_WRITE : 0),
							CREATE_NEW
						);
						if (whandle == INVALID_HANDLE_VALUE)
							error = GetLastError();
						else
							error = 0;
					}
					if (error != 0) {
						err =  boost::system::error_code(error, boost::asio::error::get_system_category());
						return;
					}
				}
				assign(whandle, err);
				return;
			}

			void open(const ext::filesystem::path& path, unsigned int mode = in|out)
			{
				boost::system::error_code err;
				open(path, mode, err);
				if (err) throw boost::system::system_error(err);
			}

			void open(const ext::filesystem::path& path, boost::system::error_code& err)
			{
				open(path, in|out, err);
			}

			template <typename Handler>
			struct handle_offset_inc {
				fstream* file;
				Handler handler;

				handle_offset_inc(fstream* file_, const Handler& handler_)
					: file(file_), handler(handler_) {};

				void operator()(const boost::system::error_code& error, std::size_t bytes_transferred)
				{
					// if error, we might be aborted because file object was destroyed, and parent is no longer valid
					// however, we don't actually support abortion for async_open, or for the non-iocp case
					// so just ignore this for now
					file->offset += bytes_transferred;
					handler(error, bytes_transferred);
				}
			};

			template <typename MBS, typename Handler>
			void async_read_some(const MBS& mbs, const Handler& handler)
			{
				async_read_some_at(offset, mbs, handle_offset_inc<Handler>(this, handler));
			}

			template <typename CBS, typename Handler>
			void async_write_some(const CBS& cbs, const Handler& handler)
			{
				if (!written) {
					// If first write after a seek, truncate file to current file position
					written = true;
					SetEndOfFile(native());
				}
				async_write_some_at(offset, cbs, handle_offset_inc<Handler>(this, handler));
			}

			void fseeka(uint64 offset_) {
				offset = offset_;
				written = false;
				// setting file pointer here for truncate purposes in async_write_some
				LARGE_INTEGER li;
				li.QuadPart = offset;
				SetFilePointer(native(), li.LowPart, &li.HighPart, FILE_BEGIN);
			}
	};

#else
	class fstream {
		private:
			boost::asio::io_service& service;
			FILE* fd;

			ext::asio::fstream_service<fstream>& get_fstream_service()
			{
				return boost::asio::use_service<ext::asio::fstream_service<fstream> >(service);
			}

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

			fstream(boost::asio::io_service& service_)
				: service(service_)
				, fd(NULL)
			{
			}

			~fstream()
			{
				close();
			}

			boost::asio::io_service& get_io_service()
			{
				return service;
			}

			void open(const ext::filesystem::path& path, unsigned int mode, boost::system::error_code& err)
			{
				std::string openmode = "";
				if (mode&out && mode&create) openmode += "w";
				else if (mode&in) openmode += "r";
				if (mode&in && mode&out) openmode += "+";
				if (!(mode&text)) openmode += "b";
				fd = ext::filesystem::fopen(path, openmode.c_str());
				if (fd != NULL)
					err = boost::system::error_code();
				else
					err = ext::posix_error::make_error_code(errno);
			}

			void open(const ext::filesystem::path& path, unsigned int mode = in|out)
			{
				boost::system::error_code err;
				open(path, mode, err);
				if (err) throw boost::system::system_error(err);
			}

			void open(const ext::filesystem::path& path, boost::system::error_code& err)
			{
				open(path, in|out, err);
			}

			template <typename Handler>
			void async_open(const ext::filesystem::path& path, int flags, const Handler& handler)
			{
				get_fstream_service().async_open(this, path, flags, handler);
			}

			void close() {
				if (fd != NULL) {
					fclose(fd);
					fd = NULL;
				}
			}

			template <typename MBS, typename Handler>
			void async_read_some(const MBS& mbs, const Handler& handler)
			{
				get_fstream_service().get_work_service().dispatch(
					helper_read_some<MBS, Handler>(service, mbs, fd, handler)
				);
			}

			template <typename CBS, typename Handler>
			void async_write_some(const CBS& cbs, const Handler& handler)
			{
				get_fstream_service().get_work_service().dispatch(
					helper_write_some<CBS, Handler>(service, cbs, fd, handler)
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

				helper_write_some(boost::asio::io_service& service_, const CBS& cbs_, FILE* fd_, const Handler& handler_)
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

				helper_read_some(boost::asio::io_service& service_, const MBS& mbs_, FILE* fd_, const Handler& handler_)
					: service(service_), mbs(mbs_), fd(fd_), handler(handler_)
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
} }

#endif//ASIO_FILE_STREAM
