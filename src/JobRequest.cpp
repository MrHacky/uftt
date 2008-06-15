#include "JobRequest.h"

#include "Logger.h"

#include <iostream>
#include <boost/filesystem/fstream.hpp>

using namespace std;
namespace fs = boost::filesystem;

// Testing crud here
#include <boost/detail/atomic_count.hpp>
#include <boost/intrusive_ptr.hpp>
using namespace boost;


class counted_base
{
private:

    mutable detail::atomic_count count_;

protected:

    counted_base(): count_( 0 ) {}
    virtual ~counted_base() {}
    counted_base( counted_base const & ): count_( 0 ) {}
    counted_base& operator=( counted_base const & ) { return *this; }

public:

    inline friend void intrusive_ptr_add_ref( counted_base const * p )
    {
        ++p->count_;
    }

    inline friend void intrusive_ptr_release( counted_base const * p )
    {
        if( --p->count_ == 0 ) delete p;
    }

    long use_count() const { return count_; }
};

void JobRequestBlobData::handleChunk(uint64 offset, uint32 len, uint8* buf)
{
	assert(false);
}

void JobRequestBlobData::handleChunk(uint32 chunknum, uint32 chunksize, uint8* buf)
{
	assert(chunksize == 1024);

	if (chunknum != curchunk) {
		LOG("don't need chunk! " << "need:" << curchunk << "  got:" << chunknum);
		return;
	}

	uint32 len = 1024;
	if (chunknum == chunkcount-1)
		len = offset;

	if (len != 0) {
		// TODO: find out why this is needed (it kills the file, but why?)
		fs::fstream fstr; // need fstream instead of ofstream if we want to append

		// TODO: hmpf. not what i wanted, but works.... find out why
		fstr.open(fpath, ios::out | ios::binary | ios::app);
		//fstr.seekp(filepos, ios_base::beg);

		fstr.write((char*)buf, len);
	}

	++curchunk;
	mtime = 0;
}
