#ifndef I_MEMORY_BUFFER_H
#define I_MEMORY_BUFFER_H

#include "Types.h"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>


/**
 * IMemoryBuffer represents a continues region of memory. It is intended as to
 * be used to subdivide regions of a larger buffer (managed by some other
 * class) to other users, while keeping track of which regions are in use.
 * IMemoryBuffers are returned for example by IFileReader's get_buffer() method.
 */
class IMemoryBuffer {
	public:
		IMemoryBuffer(uint8*const data_, const size_t size_)
		: data(data_)
		, size(size_)
		{};
		uint8*const data;    ///< data is a pointer to the data region managed by this buffer.
		const size_t size;   ///< size is the length of the data region. In particular data[i] shall be valid for all 0 <= i < size.
		virtual void finish() = 0;    ///< finish() indicates that no more accesses will occur to this buffer, and that it may be discarded. E.g. after calling finish(), data may no longer point to a valid region of memory. Implementations of finish() are encouraged to enforce calling finish() before detruction by for example setting and testing a boolean in finish and the destructor respectively.
		virtual ~IMemoryBuffer() {}; ///< The destructor can be used to detect when a buffer is discarded without a prior invokation of finish(), which indicates either a programmer error, or a fatal error in UFTT (i.e. UFTT is crashing).
};
typedef boost::shared_ptr<IMemoryBuffer> IMemoryBufferRef;
typedef const boost::shared_ptr<IMemoryBuffer> IMemoryBufferConstRef;

///**
 //* bla todo
 //*/
//struct buffer_info {
	//uint8*const data;
	//const size_t size;
//};

#endif // I_MEMORY_BUFFER_H
