#ifndef I_FILE_READER_H
#define I_FILE_READER_H

#include "util/Filesystem.h"
#include "IMemoryBuffer.h"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

class IFileReader {
	public:
		virtual void seek_to(uint64 position, const boost::function<void(bool)>&) = 0; /// true iff seek success
		virtual void read(size_t len, const boost::function<void(IMemoryBufferRef, size_t, boost::system::error_code ec)>& f) = 0;
};
typedef boost::shared_ptr<IFileReader> IFileReaderRef;
typedef const boost::shared_ptr<IFileReader> IFileReaderConstRef;

class IFileReaderManager {
	public:
		virtual void get_file_reader(const ext::filesystem::path& file, const boost::function<void(IFileReaderRef,boost::system::error_code)>&) = 0;
		virtual void set_total_buffer_size(size_t size) = 0;
};
typedef boost::shared_ptr<IFileReaderManager> IFileReaderManagerRef;
typedef const boost::shared_ptr<IFileReaderManager> IFileReaderManagerConstRef;

#endif // I_FILE_READER_H
