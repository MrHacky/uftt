#ifdef BUILD_STRING_H
#error "include only once!"
#endif

#define BUILD_STRING_H

//#define BUILD_STRING ""

#if defined(WIN32) || defined(__WIN32__)
//#define BUILD_STRING_PLATFORM "win32"
#define BUILD_STRING_PLATFORM "win32.nt"
#elif defined(__linux__)
#define BUILD_STRING_PLATFORM "linux.i386"
#else
#define BUILD_STRING_PLATFORM "unknown"
#endif

#if defined(LINK_STATIC_RUNTIME)
#define BUILD_STRING_LINKAGE "-static"
#else
#define BUILD_STRING_LINKAGE ""
#endif

#include <BuildVersion.h>

#ifndef BUILD_STRING_EXTRA
#define BUILD_STRING_EXTRA ""
#endif

#define BUILD_STRING_VERSION "-" VERSION_MAJOR_STRING "." VERSION_MINOR_STRING "." VERSION_REV_STRING

#define BUILD_STRING ("uftt-" BUILD_STRING_PLATFORM BUILD_STRING_LINKAGE BUILD_STRING_EXTRA BUILD_STRING_VERSION)

extern char build_string_macro_date[];
extern char build_string_macro_time[];
