#ifndef EXT_FILESYSTEM_H
#define EXT_FILESYSTEM_H

#include <boost/filesystem.hpp>

namespace ext {
	namespace filesystem {
		inline bool exists(const boost::filesystem::path& ph) {
			try {
				return boost::filesystem::exists(ph);
			} catch (...) {
				return false;
			}
		}

		inline bool exists(const boost::filesystem::wpath& ph) {
			try {
				return boost::filesystem::exists(ph);
			} catch (...) {
				return false;
			}
		}
	}
}

#endif//EXT_FILESYSTEM_H
