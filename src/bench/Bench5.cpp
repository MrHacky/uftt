#include <iostream>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifdef __linux__
#include <sys/mman.h>
#include "../net-asio/asio_file_stream.h"
#endif
boost::asio::io_service svc;
services::diskio_service ds(svc);
#ifdef WIN32
	#define NUM_FILES 6
#else
	#define NUM_FILES 8
#endif
boost::filesystem::path files[NUM_FILES];
size_t curfile = 0;
boost::posix_time::ptime prevtime;
char buf[1024*1024*10];
char buf2[1024*1024*10];
//size_t bufsize = 1024*32;
size_t bufsize = 1024*512;
//size_t bufsize = 1024*1024;
//size_t bufsize = 10*1024*1024;
size_t sum;

#ifdef WIN32
	class diskio_filetype: public boost::asio::windows::random_access_handle {
		private:
			//void assign(const native_type & native_descriptor);
			//boost::system::error_code assign(const native_type & native_descriptor,	boost::system::error_code & ec);

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

			diskio_filetype(boost::asio::io_service& service)
				: boost::asio::windows::random_access_handle(service)
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
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
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
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN,
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
				assign(whandle);
				return boost::system::error_code();
			}
	};
#else
	class diskio_filetype: public services::diskio_filetype {
		private:

		public:
			diskio_filetype(boost::asio::io_service& service)
				: services::diskio_filetype(ds)
			{
			}

			template<typename Ta, typename Tb>
			void async_read_some_at(boost::uint64_t size, const Ta& buffers, const Tb& handler) {
				async_read_some(buffers, handler);
			}
	};
#endif

void handle_read(diskio_filetype* file, char* buf, char* prevbuf, size_t bufsize, boost::uint64_t pos, const boost::system::error_code& ec, size_t len);

void handle_file_done(diskio_filetype* file)
{
	boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
	if (file) {
		std::cout << "Sum: " << sum << "\n";
		std::cout << "Time elapsed for file[" << curfile++ << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		delete file;
	}
	prevtime = time;
	sum = 0;

	if (curfile >= NUM_FILES ) {
		svc.stop();
		return;
	}
	file = new diskio_filetype(svc);
	file->open(files[curfile]);
	handle_read(file, buf, buf2, bufsize, 0, boost::system::error_code(), 0);
}

void handle_read(diskio_filetype* file, char* buf, char* prevbuf, size_t bufsize, boost::uint64_t pos, const boost::system::error_code& ec, size_t len)
{
	if (ec) {
		//std::cout << "Error occured: " << ec.message() << "\n";
		handle_file_done(file);
		return;
	}

	pos += len;
	if (!ec)
		file->async_read_some_at(
		//boost::asio::async_read_at(
			pos, boost::asio::buffer(buf, bufsize),
			boost::bind(&handle_read, file, prevbuf, buf, bufsize, pos, _1, _2)
		);

	if (len > 0) {
		for (size_t i = 0; i < len; ++i)
			sum += prevbuf[i];
	}
}

void sum_async_read_some_at()
{
	std::cout << "\nsum_async_read_some_at:\n\n";
	handle_file_done(NULL);
	boost::asio::io_service::work work(svc);
	svc.run();
}

#ifdef WIN32
void sum_win_mmap()
{
	std::cout << "\nsum_win_mmap:\n\n";
	prevtime = boost::posix_time::microsec_clock::universal_time();
	for (int i = 0; i < NUM_FILES; ++i) {
		size_t fsize = boost::filesystem::file_size(files[i]);
		std::string fname = files[i].native_file_string();
				HANDLE whandle = ::CreateFile(
					TEXT(fname.c_str()),
					GENERIC_READ,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,// | FILE_FLAG_OVERLAPPED,
					NULL
				);
		HANDLE fmapping = CreateFileMapping(
			whandle,
			NULL,
			PAGE_READONLY,
			0,
			fsize,
			NULL
		);
		char* mapbuf = (char*)MapViewOfFile(fmapping, FILE_MAP_READ, 0, 0, fsize);
		sum = 0;
		for (size_t j = 0; j < fsize; ++j)
			sum += mapbuf[j];
		UnmapViewOfFile(mapbuf);
		CloseHandle(fmapping);
		CloseHandle(whandle);
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Sum: " << sum << "\n";
		std::cout << "Time elapsed for file[" << i << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		prevtime = time;
	}
}
#endif // WIN32

#ifdef __linux__
void sum_linux_mmap() {
		std::cout << "\nsum_linux_mmap:\n\n";
	prevtime = boost::posix_time::microsec_clock::universal_time();
	for (int i = 0; i < NUM_FILES; ++i) {
		size_t fsize = boost::filesystem::file_size(files[i]);
		std::string fname = files[i].native_file_string();
		int fd = open(fname.c_str(), O_RDONLY);
		char* mapbuf = (char*)mmap(NULL, fsize, PROT_READ, MAP_SHARED, fd, 0);
		sum = 0;
		for (size_t j = 0; j < fsize; ++j)
			sum += mapbuf[j];
		munmap(mapbuf, fsize);
		close(fd);
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Sum: " << sum << "\n";
		std::cout << "Time elapsed for file[" << i << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		prevtime = time;
	}
}
#endif

void sum_fread()
{
	std::cout << "\nsum_fread:\n\n";
	prevtime = boost::posix_time::microsec_clock::universal_time();
	for (int i = 0; i < NUM_FILES; ++i) {
		FILE* file = fopen(files[i].native_file_string().c_str(), "rb");
		setbuf(file, NULL);
		sum = 0;
		while (size_t read = fread(buf, 1, bufsize, file))
		{
			for (size_t j = 0; j < read; ++j)
				sum += buf[j];
		}
		fclose(file);
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Sum: " << sum << "\n";
		std::cout << "Time elapsed for file[" << i << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		prevtime = time;
	}
}

void flush_cashes() {
	FILE* f = fopen("/proc/sys/vm/drop_caches", "wb");
	std::cout << "Flushing cashes... ";
	if(f) {
		fwrite("1\n", 1, 2, f);
		fwrite("2\n", 1, 2, f);
		fwrite("3\n", 1, 2, f);
		std::cout << "ok." << std::endl;
		fclose(f);
	}
	else {
		std::cout << "FAILED!" << std::endl;
	}
}

int main(int argc, char** argv)
{
	#ifdef WIN32
	files[0] = "D:/temp/UFTT/file0.bin";
	files[1] = "D:/temp/UFTT/file1.bin";
	files[2] = "D:/temp/UFTT/file2.bin";
	files[3] = "D:/temp/UFTT/file3.bin";
	files[4] = "D:/temp/UFTT/file4.bin";
	files[5] = "D:/temp/UFTT/file5.bin";
	sum_fread();
	sum_async_read_some_at();
	sum_win_mmap();
	Sleep(5000);
#else
	files[0] = "/home/dafox/anime/lgtv";
	files[1] = "/home/dafox/anime/Naruto_intro.srt";
	files[2] = "/home/dafox/anime/webaom.jar";
	files[3] = "/home/dafox/3dlogic.tar.bz2";
	files[4] = "/home/dafox/firefox_persona_test.xcf";
	files[5] = "/home/dafox/sharedfolder/bcp/gAME/winkawaks145.rar";
	files[6] = "/home/dafox/sharedfolder/bcp/gAME/neoragex.rar";
	//files[7] = "/home/dafox/anime/[Rabbit-Force]_Clannad_-After_Story-_[DVD_H264_AAC]/[Rabbit-Force]_Clannad_-After_Story-_25_-_Kyou_Arc_[DVD_H264_AAC][E960BADF].mkv";
	files[7] = "/media/disk/[DB]_Naruto_Shippuuden_Movie_3_[BCC77C3B].avi";
	flush_cashes();
	sum_async_read_some_at();
	flush_cashes();
	sum_fread();
	flush_cashes();
	sum_linux_mmap();
	//flush_cashes();
	//sum_async_read_some_at();
	#endif
};

#if 0
class coroutine
{
public:
	coroutine() : value(-1) {}
private:
	friend class coroutine_ref;
	int value;
};

class coroutine_ref
{
public:
	coroutine_ref(coroutine& c) : value(c.value) {}
	coroutine_ref(coroutine* c) : value(c->value) {}
	operator bool() const { return value == -1; }
	int get() { return value; };
	bool set(int v) {
		value = v;
		return false;
	}
private:
  int& value;
};

#define CR_REENTER(c) \
	if (false) { \
		label_bail_out_of_coroutine_: ; \
	} else if (coroutine_ref coro_value_ = c) { \
		goto label_start_of_coroutine_; \
	} else \
		switch (coro_value_.get()) label_start_of_coroutine_:

#define CR_YIELD \
	if (coro_value_.set(__LINE__)) { \
		case __LINE__: ; \
	} else \
		for (bool coro_bool_ = false; ; coro_bool_ = !coro_bool_) \
			if (coro_bool_) \
				goto label_bail_out_of_coroutine_; \
			else

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>

using boost::shared_ptr;
using boost::array;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::system;


class session : coroutine
{
public:
  session(tcp::acceptor& acceptor)
    : acceptor_(acceptor),
      socket_(new tcp::socket(acceptor.get_io_service())),
      data_(new array<char, 1024>)
  {
  }

  void operator()(
      error_code ec = error_code(),
      size_t length = 0)
  {
    CR_REENTER (this)
    {
      for (;;)
      {
        CR_YIELD acceptor_.async_accept(
            *socket_, *this);

        while (!ec)
        {
          CR_YIELD socket_->async_read_some(
              buffer(*data_), *this);

          if (ec) break;

          CR_YIELD async_write(*socket_,
              buffer(*data_, length), *this);
        }

        socket_->close();
      }
    }
  }

private:
  tcp::acceptor& acceptor_;
  shared_ptr<tcp::socket> socket_;
  shared_ptr<array<char, 1024> > data_;
};

#include <iostream>

int main(int argc, char** argv)
{
	boost::asio::io_service service;
	tcp::acceptor acceptor(service);
	int a = 10;

	bool done = false;
	coroutine coro;
	while (!done) {
		CR_REENTER(coro) {
			std::cout << "a\n";
			std::cout << "b\n";
			CR_YIELD std::cout << "c\n";
			std::cout << "d\n";
			CR_YIELD std::cout << "e\n";
			CR_YIELD std::cout << "f\n";
			CR_YIELD std::cout << "g\n";
			std::cout << "h\n";
			//CR_YIELD;
			std::cout << "i\n";	class diskio_filetype: public boost::asio::windows::random_access_handle {
		private:
			//void assign(const native_type & native_descriptor);
			//boost::system::error_code assign(const native_type & native_descriptor,	boost::system::error_code & ec);

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

			diskio_filetype(boost::asio::io_service& service)
				: boost::asio::windows::random_access_handle(service)
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
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
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
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN,
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
				assign(whandle);
				return boost::system::error_code();
			}
	};
			done = true;
		}
		std::cout << "yield\n";
	}
}
#endif
