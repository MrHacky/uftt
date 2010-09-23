#include "FReadFileReader.h"
#include "FReadMemoryBuffer.h"
#include "util/PosixError.h"
#include <iostream>
#include <stdio.h>

using namespace std;

/********************************
 * Helpers                      *
 ********************************/

struct FReadFileReaderManager::helper_open_file {
	FReadFileReaderManager* frm;
	ext::filesystem::path path;
	boost::function<void(IFileReaderRef,boost::system::error_code)> handler;

	helper_open_file(FReadFileReaderManager* frm_, ext::filesystem::path path_, const boost::function<void(IFileReaderRef,boost::system::error_code)>& handler_)
	: frm(frm_)
	, path(path_)
	, handler(handler_)
	{};

	void operator()()
	{
		boost::system::error_code ec;
		IFileReaderRef ref;
		FILE* fd = ext::filesystem::fopen(path, "rb");
	
		if (fd)
			ref = IFileReaderRef(new FReadFileReader(frm, fd));
		else
			ec = ext::posix_error::make_error_code(errno);

		frm->get_io_service().dispatch(boost::bind(handler,
			ref,
			ec
		));
	}
};

struct FReadFileReaderManager::helper_open_file_wrapper {
	helper_open_file& ref;

	helper_open_file_wrapper(helper_open_file& ref_)
		: ref(ref_) {};

	void operator()()
	{
		ref();
	}
};

struct FReadFileReaderManager::helper_read_some {
	FReadFileReader* fr;
	size_t len;
	IMemoryBufferRef buf;
	const boost::function<void(IMemoryBufferRef, size_t, boost::system::error_code ec)> handler;

	//pre: fd is valid
	helper_read_some(FReadFileReader* fr_, size_t len_, const boost::function<void(IMemoryBufferRef, size_t len, boost::system::error_code ec)>& handler_)
	: len(len_)
	, fr(fr_)
	, handler(handler_)
	{}

	void operator()()
	{
		size_t read = fread(buf->data, 1, buf->size, fr->fd);

		fr->manager->get_io_service().dispatch(
			boost::bind(
				handler,
				buf,
				read,
				ext::posix_error::make_error_code(ferror(fr->fd))
			)
		);

		delete this; // slightly ugly
	}

	typedef void result_type;

	private:
		helper_read_some(const helper_read_some& o) {}; // prevent copy construct
		helper_read_some& operator=(const helper_read_some& o) {} // prevent assignment
};

struct FReadFileReaderManager::helper_read_some_wrapper {
	helper_read_some& ref;

	helper_read_some_wrapper(helper_read_some& ref_)
		: ref(ref_) {};

	void operator()()
	{
		ref();
	}
};

FReadFileReaderManager::FReadFileReaderManager(boost::asio::io_service& io_service_)
: io_service(io_service_)
, work(work_io_service)
, work_thread(boost::bind(&boost::asio::io_service::run, &work_io_service))
, buffer_begin(0)
, buffer_usage(0)
, buffer(0)
, buffer_size(32*1024*1024)
{
}

void FReadFileReaderManager::set_total_buffer_size(size_t size) {
	buffer_size = size; // actual resize happens as soon as needed/possible in get_buffer or release_buffer
}

boost::asio::io_service& FReadFileReaderManager::get_io_service() {
	return io_service;
}

boost::asio::io_service& FReadFileReaderManager::get_work_service() {
	return work_io_service;
}

bool FReadFileReaderManager::check_buffer_resize() {
	if(   (buffer_size != buffer.size())                // Intended size != current size
	   && (buffer_begin + buffer_usage <= buffer.size()) // Currently not wrapped
	   && (buffer_begin + buffer_usage <= buffer_size)   // In-use still fits in requested
	)
	{
		buffer.resize(buffer_size);
	}
	return buffer_size == buffer.size();
}

IMemoryBufferRef FReadFileReaderManager::get_buffer(size_t size) {
	IMemoryBufferRef buf;

	if(!check_buffer_resize()) {
		return buf;
	}

	size_t alloc = min(size, buffer.size() - buffer_usage);
	if(buffer_begin + buffer_usage         < buffer.size() &&
	   buffer_begin + buffer_usage + alloc > buffer.size()) {
		// if this happens we would allocate a section of buffer which wraps
		alloc = min(alloc, buffer.size() - (buffer_begin + buffer_usage));
	}

	if(alloc == 0) { // No space available
		return buf;
	}

	buf = IMemoryBufferRef(
		new  FReadMemoryBuffer(
			&buffer[(buffer_begin + buffer_usage) % buffer.size()],
			alloc,
			this
		)
	);

	buffer_usage += alloc;

	return buf;
}

void FReadFileReaderManager::release_buffer(FReadMemoryBuffer*const buffer)
{
	size_t offset = buffer->data - &this->buffer[0];
	if (buffer_begin == offset) {
		buffer_begin += buffer->size;
		buffer_usage -= buffer->size;
		std::map<size_t, size_t>::iterator i;
		while ((i = released_buffer_sections.find(offset)) != released_buffer_sections.end()) {
			buffer_begin += i->second;
			buffer_usage -= i->second;
			released_buffer_sections.erase(i);
		}
	} else {
		released_buffer_sections[offset] = buffer->size;
	}

	if (!read_request_queue.empty()) {
		read_request_queue[0]->buf = get_buffer(read_request_queue[0]->len); // TODO: maybe wait for sufficient space?
		if (read_request_queue[0]->buf) {
			get_work_service().dispatch(helper_read_some_wrapper(*read_request_queue[0]));
			read_request_queue.pop_front();
		}
	}
}


void FReadFileReaderManager::get_file_reader(const ext::filesystem::path& file, const boost::function<void(IFileReaderRef,boost::system::error_code)>& f)
{
	helper_open_file* helper = new helper_open_file(this, file, f);
	get_work_service().dispatch(helper_open_file_wrapper(*helper));
}

FReadFileReader::FReadFileReader(FReadFileReaderManager*const manager_, FILE* fd_)
: manager(manager_)
, fd(fd_)
{
}

/*
FReadFileReader::FReadFileReader(const ext::filesystem::path path_, const uint32 id_, FReadFileReaderManager*const manager_)
: id(id_)
, manager(manager_)
{
	fd = fopen(path_.string().c_str(), "rb");
	if (int error = ferror(fd)) {
		std::cout << "Warning: FReadFileReader failed to open filesystem object for reading." << std::endl;
		throw boost::system::system_error(ext::posix_error::make_error_code(error));
	}
}
*/

void FReadFileReader::seek_to(const uint64 position, const boost::function<void(bool)>& f)
{
}


void FReadFileReader::read(const size_t len, const boost::function<void(IMemoryBufferRef, size_t, boost::system::error_code ec)>& handler) {
	FReadFileReaderManager::helper_read_some* helper = new FReadFileReaderManager::helper_read_some(this, len, handler);
	helper->buf = manager->get_buffer(len);
	if (helper->buf)
		manager->get_work_service().dispatch(FReadFileReaderManager::helper_read_some_wrapper(*helper));
	else
		manager->read_request_queue.push_back(helper);
}

FReadFileReader::~FReadFileReader() {
	if(fd) {
		fclose(fd);
		fd = NULL;
	}
}


