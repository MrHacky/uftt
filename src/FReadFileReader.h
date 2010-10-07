#ifndef FREAD_FILE_READER_H
#define FREAD_FILE_READER_H

#include "IFileReader.h"
#include "IMemoryBuffer.h"
#include "FReadMemoryBuffer.h"
#include "util/Filesystem.h"
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <vector>
#include <map>
#include <utility>
#include <list>

class FReadMemoryBuffer;

/**
 * FReadFileManager provides access and manages FReadFileReaders. Only one
 * instance of FReadFileManager should be instantiated at a time.
 */
class FReadFileReaderManager : public IFileReaderManager {
	protected:
		friend class FReadFileReader;

		boost::asio::io_service&      io_service;
		boost::asio::io_service       work_io_service;
		boost::asio::io_service::work work;
		boost::thread                 work_thread;

		size_t                        buffer_begin;
		size_t                        buffer_usage;
		size_t                        buffer_size; ///< requested size of buffer
		std::vector<uint8>            buffer;
		//boost::shared_ptr<std::set<std::pair<size_t, size_t>, compare_ding > > freed_buffer_sections;

		//static boost::function<bool(std::pair<size_t, size_t>,std::pair<size_t, size_t>)> f;
		//std::set<buffer_info> released_buffer_sections;
		std::map<size_t, size_t> released_buffer_sections;
		friend class FReadMemoryBuffer;
		void release_buffer(FReadMemoryBuffer* const buf);
		bool check_buffer_resize();

		struct helper_read_some;
		struct helper_read_some_wrapper;
		std::deque<helper_read_some*> read_request_queue; ///< read request which could not get a buffer

		struct helper_open_file;
		struct helper_open_file_wrapper;

	public:
		FReadFileReaderManager(boost::asio::io_service& io_service_);
		void get_file_reader(const ext::filesystem::path& file, const boost::function<void(IFileReaderRef,boost::system::error_code)>& f);

		void set_total_buffer_size(size_t size);

		void get_buffer(size_t size, const boost::function<void(IMemoryBufferRef)>&); // async version
		IMemoryBufferRef get_buffer(size_t size); // non async version

		boost::asio::io_service& get_io_service();
		boost::asio::io_service& get_work_service();
};

class FReadFileReader : public IFileReader {
	public:
		//FReadFileReader(const ext::filesystem::path path_, const uint32 id_, FReadFileReaderManager*const manager_);
		FReadFileReader(FReadFileReaderManager*const manager_, FILE* fd_);
		void seek_to(uint64 position, const boost::function<void(bool)>& f);
		void read(size_t len, const boost::function<void(IMemoryBufferRef, size_t, boost::system::error_code ec)>& f);
		~FReadFileReader();

	private:
		//uint32 id;
		FReadFileReaderManager*const manager;
		FILE* fd;

		friend FReadFileReaderManager::helper_read_some;
};

#endif // FREAD_FILE_READER_H
