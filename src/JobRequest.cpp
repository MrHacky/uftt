#include "JobRequest.h"

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

