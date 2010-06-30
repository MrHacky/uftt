#include "BuildString.h"

#include <sstream>
#include <boost/foreach.hpp>

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#  if defined(_WIN32_WINDOWS)
#    if _MSC_VER >= 1500
#      error "Visual Studio 2008+ does not support OS < XP"
#    endif
#    define BUILD_STRING_PLATFORM "win32.9x"
#  else
#    define BUILD_STRING_PLATFORM "win32.nt"
#  endif
#elif defined(__linux__)
#  if defined(__LP64__)
#    define BUILD_STRING_PLATFORM "linux.x86_64"
#  else
#    define BUILD_STRING_PLATFORM "linux.i386"
#  endif
#else
#  define BUILD_STRING_PLATFORM "unknown"
#endif

#if defined(LINK_STATIC_RUNTIME)
#define BUILD_STRING_LINKAGE "-static"
#else
#define BUILD_STRING_LINKAGE ""
#endif

#ifndef BUILD_STRING_EXTRA
#define BUILD_STRING_EXTRA ""
#endif

#define BUILD_STRING_VERSION "-" VERSION_MAJOR_STRING "." VERSION_MINOR_STRING "." VERSION_PATCH_STRING

#define BUILD_STRING ("uftt-" BUILD_STRING_PLATFORM BUILD_STRING_LINKAGE BUILD_STRING_EXTRA BUILD_STRING_VERSION)

extern char build_string_macro_date[];
extern char build_string_macro_time[];

extern char build_string_version_major[];
extern char build_string_version_minor[];
extern char build_string_version_patch[];
extern char build_string_version_extra[];

std::string get_build_string()
{
	std::string bstring = "uftt-";
	bstring += BUILD_STRING_PLATFORM;
	bstring += BUILD_STRING_LINKAGE;
	if (strlen(build_string_version_extra) > 0) {
		if (build_string_version_extra[0] != '-') bstring += '-';
		bstring += build_string_version_extra;
	}
	bstring += '-';
	bstring += build_string_version_major;
	bstring += '.';
	bstring += build_string_version_minor;
	bstring += '.';

	std::string pstring = build_string_version_patch;
	if (pstring == "<TIMESTAMP>") {
		std::stringstream sstamp;

		int month, day, year;
		{
			std::string tstring = build_string_macro_date;
			char* temp = &tstring[0];

			// ANSI C Standard month names
			const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
			year = atoi(temp + 7);
			*(temp + 6) = 0;
			day = atoi(temp + 4);
			*(temp + 3) = 0;
			for (month = 0; month < 12; ++month)
				if (!strcmp(temp, months[month]))
					break;
			++month;
		}

		std::string tstamp(build_string_macro_time);
		BOOST_FOREACH(char& chr, tstamp)
			if (chr == ':') chr = '_';

		sstamp << year << '_' << month << '_' << day << '_' << tstamp;

		pstring = sstamp.str();
	}
	bstring += pstring;
	return bstring;
}
