#include "FReadMemoryBuffer.h"
#include "FReadFileReader.h"

FReadMemoryBuffer::FReadMemoryBuffer(uint8*const data_, const size_t size_, FReadFileReaderManager*const manager_)
: IMemoryBuffer(data_, size_)
, finished(false)
, manager(manager_)
{
}

void FReadMemoryBuffer::finish()
{
	manager->release_buffer(this);
	finished = true;
}

FReadMemoryBuffer::~FReadMemoryBuffer()
{
	assert(finished);
}
