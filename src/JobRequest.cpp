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

uint32 buftest[256];

void JobRequestBlobData::handleChunk(uint32 chunknum, uint32 chunksize, uint8* buf)
{
	assert(chunksize == 1024);

	uint32 cbi = chunknum&0xff;
	if (chunknum != curchunk) {
		LOG("don't need chunk! " << "need:" << curchunk << "  got:" << chunknum);
		if (chunknum < curchunk) {
			LOG("AND its into the past!");
		} else if (chunknum < curchunk+255) {
			if (!usebuf[cbi]) {
				usebuf[cbi] = true;
				buftest[cbi] = chunknum;
				memcpy(buffer[cbi], buf, 1024);
			} else {
				LOG("AND we already have it!");
			}
			mtime = 0;
		} else { 
			LOG("AND it is too far into the future!");
		}
		return;
	} else 
		assert(!usebuf[cbi]);

	uint32 len = 1024;
	if (chunknum == chunkcount-1)
		len = offset;

	++curchunk; ++cbi; cbi &= 0xff;

	if (len != 0) {
		// TODO: find out why this is needed (it kills the file, but why?)
		fs::fstream fstr; // need fstream instead of ofstream if we want to append

		// TODO: hmpf. not what i wanted, but works.... find out why
		if (chunknum == 0)
			fstr.open(fpath, ios::out | ios::binary);
		else
			fstr.open(fpath, ios::out | ios::binary | ios::app);
		//fstr.seekp(filepos, ios_base::beg);

		fstr.write((char*)buf, len);

		while (usebuf[cbi]) {
			if (curchunk == chunkcount-1) len = offset;
			if (len != 0) fstr.write((char*)buffer[cbi], len);
			LOG("Used chunk from buffer:" << curchunk);
			assert(buftest[cbi] == curchunk);
			usebuf[cbi] = false;
			++curchunk; ++cbi; cbi &= 0xff;
		}
	}

	mtime = 0;
}

bool JobRequestBlobData::timeout(uint32& reqchunk, uint8& reqcnt)
{
	reqchunk = curchunk;
	reqcnt = 8;
	return true;
}
