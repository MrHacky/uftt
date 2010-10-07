#include "FReadMemoryBuffer.h"
#include "FReadFileReader.h"

FReadMemoryBuffer::FReadMemoryBuffer(uint8*const data_, const size_t size_, FReadFileReaderManager*const manager_)
: IMemoryBuffer(data_, size_)
, manager(manager_)
, finished(false)
{
}

void FReadMemoryBuffer::finish()
{
	manager->release_buffer(this);
	finished = true;
}

FReadMemoryBuffer::~FReadMemoryBuffer()
{
	if(!finished) {
		//FIXME: pick a better error if protocol_error (EPROTO) is not appropriate
		assert(false);
		throw boost::system::system_error(boost::system::error_code(boost::system::errc::protocol_error, boost::system::get_system_category()));
	}
}
