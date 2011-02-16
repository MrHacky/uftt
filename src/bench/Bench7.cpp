//#include <iostream>
#include <algorithm>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../net-asio/asio_file_stream.h"

#include "../FReadFileReader.h"

#ifdef __linux__
#include <sys/mman.h>
#endif
boost::asio::io_service svc;
services::diskio_service ds(svc);
#define MAX_FILES 100
#ifdef WIN32
	int NUM_FILES = 6;
#else
	int NUM_FILES = 1;
#endif
#define TGT_FILESIZE (512*1024*1024)
boost::filesystem::path files[MAX_FILES];
size_t curfile = 0;
boost::posix_time::ptime prevtime;
#define PAGE_SIZE (4*1024)
#define ALIGN(b, a) ((void*)(((uintptr_t)b+(a)) & -(a)));
char buf1[1024*1024*10 + PAGE_SIZE];
char buf2[1024*1024*10];
char* buf = (char*)ALIGN(buf1, PAGE_SIZE);

//size_t bufsize = 1024*32;
size_t bufsize = 1024*512;
//size_t bufsize = 1024*1024;
//size_t bufsize = 10*1024*1024;
size_t sum;

void clear( boost::asio::io_service& service )
{
    service.stop();
    service.~io_service();
    new( &service ) boost::asio::io_service;
}


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

#ifdef __linux__0
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

#if 0
void bench_write()
{
	std::cout << "bench_fwrite:\n\n";
	prevtime = boost::posix_time::microsec_clock::universal_time();
	for (int i = 0; i < NUM_FILES; ++i) {
		int file = open(files[i].native_file_string().c_str(), O_WRONLY | O_CREAT | O_DIRECT | O_TRUNC);
		sum = 0;
		size_t todo = TGT_FILESIZE;
		do
		{
			for (size_t j = 0; j < bufsize; ++j)
				buf[j] += buf[j-1] ^ buf[j];
			todo -= write(file, buf, std::min(bufsize, todo));
		} while(todo > 0);
		close(file);
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Time elapsed for file[" << i << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		prevtime = time;
	}
}
#endif

void bench_fread()
{
	std::cout << "bench_fread:\n\n";
	prevtime = boost::posix_time::microsec_clock::universal_time();
	for (int i = 0; i < NUM_FILES; ++i) {
		FILE* file = fopen(files[i].native_file_string().c_str(), "rb");
		sum = 0;
		size_t todo = TGT_FILESIZE;
		do
		{
			todo = fread(buf, 1, bufsize, file);
			for (size_t j = 0; j < todo; ++j)
				sum += buf[j];
		} while(todo > 0);
		fclose(file);
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Sum: " << sum << "\n";
		std::cout << "Time elapsed for file[" << i << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
		prevtime = time;
	}
}

void frm_file_done(IFileReaderManagerRef frm, int filenum);

void frm_doread(IFileReaderManagerRef frm, int filenum, IFileReaderRef ifrr, IMemoryBufferRef imbr, size_t len, boost::system::error_code ec)
{
	if (!imbr || len > 0)
		ifrr->read(bufsize, boost::bind(&frm_doread, frm, filenum, ifrr, _1, _2, _3));

	if (len > 0) {
		char* buf = (char*)imbr->data;
		for (int i = 0; i < len; ++i)
			sum += buf[i];
	}

	if (!imbr || len > 0) /*empty*/;
	else {
		std::cout << "Sum: " << sum << "\n";
		frm_file_done(frm, filenum);
	}

	if (imbr)
		imbr->finish();
}

void frm_file_open(IFileReaderManagerRef frm, int filenum, IFileReaderRef ifrr, boost::system::error_code ec)
{
	sum = 0;
	if (ec)
		std::cout << "Open fail: " << ec.message() << "\n";
	else
		frm_doread(frm, filenum, ifrr, IMemoryBufferRef(), 0, boost::system::error_code());
}

void frm_file_done(IFileReaderManagerRef frm, int filenum)
{
	if (filenum >= 0) {
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::universal_time();
		std::cout << "Time elapsed for file[" << filenum << "]: " << (time-prevtime).total_microseconds() / 1000000.0 << "\n";
	}
	prevtime = boost::posix_time::microsec_clock::universal_time();
	++filenum;
	if (filenum < NUM_FILES) {
		frm->get_file_reader(files[filenum].string(), boost::bind(&frm_file_open, frm, filenum, _1, _2));
	} else
		svc.stop();

}

void bench_frm()
{
	clear(svc);
	std::cout << "\nbench_frm:\n\n";
	IFileReaderManagerRef frm(new  FReadFileReaderManager(svc));

	frm_file_done(frm, -1);

	boost::asio::io_service::work wrk(svc);
	svc.run();
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
	NUM_FILES = 8;
	files[0] = "D:/temp/UFTT/file0.bin";
	files[1] = "D:/temp/UFTT/file1.bin";
	files[2] = "D:/temp/UFTT/file2.bin";
	files[3] = "D:/temp/UFTT/file3.bin";
	files[4] = "D:/temp/UFTT/file4.bin";
	files[5] = "D:/temp/UFTT/file5.bin";
	files[6] = "D:/temp/UFTT/file6.bin";
	files[7] = "D:/temp/UFTT/file7.bin";
	files[8] = "D:/temp/UFTT/file8.bin";
	files[9] = "D:/temp/UFTT/file9.bin";
	files[10] = "D:/temp/UFTT/file10.bin";
	files[11] = "D:/temp/UFTT/file11.bin";

	bench_fread();
	Sleep(2000);
	bench_fread();
	Sleep(2000);
	sum_async_read_some_at();
	Sleep(2000);
	bench_frm();
	Sleep(2000);
	sum_win_mmap();
	Sleep(2000);
#else
	files[0] = "/tmp/bench-7.bin";
	//flush_cashes();
	//sum_async_read_some_at();
	flush_cashes();
	bench_fwrite();
	//flush_cashes();
	//sum_linux_mmap();
	#endif
};
