#ifndef QEXT_PLATFORM_H
#define QEXT_PLATFORM_H

#include <string>

namespace qext {
	namespace platform {
		std::string convertUTF8ToLocale(const std::string& src);
		std::string convertLocaleToUTF8(const std::string& src);
	}
}

#endif//QEXT_PLATFORM_H
