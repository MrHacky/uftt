#ifndef FREAD_MEMORY_BUFFER_H
#define FREAD_MEMORY_BUFFER_H

#include "IMemoryBuffer.h"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

class FReadFileReaderManager;

class FReadMemoryBuffer : public IMemoryBuffer {
	public:
		FReadMemoryBuffer(uint8*const data_, const size_t size_, FReadFileReaderManager*const manager_);
		virtual void finish();
		~FReadMemoryBuffer();

		/* 'Private' interface to FReadFileReaderManager */
		bool finished;                        ///< Should be set to true by calling finish(), before the buffer is destroyed.
		FReadFileReaderManager*const manager; ///< The manager that owns this buffer.
};
typedef boost::shared_ptr<FReadMemoryBuffer> FReadMemoryBufferRef;
typedef const boost::shared_ptr<FReadMemoryBuffer> FReadMemoryBufferConstRef;

#endif // FREAD_MEMORY_BUFFER_H
