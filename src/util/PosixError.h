#ifndef EXT_POSIX_ERROR_H
#define EXT_POSIX_ERROR_H

#include <boost/version.hpp>
#include <boost/system/error_code.hpp>

namespace ext {
	namespace posix_error {
		inline boost::system::error_code make_error_code(int err)
		{
#if BOOST_VERSION <= 103500
				boost::system::posix_error::posix_errno errcode = (boost::system::posix_error::posix_errno)err;
#else
				boost::system::posix_error::errc_t errcode = (boost::system::posix_error::errc_t)err;
#endif
				return boost::system::posix_error::make_error_code(errcode);
		}
	}
}

#endif//EXT_POSIX_ERROR_H
