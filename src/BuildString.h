#ifdef BUILD_STRING_H
#error "include only once!"
#endif

#define BUILD_STRING_H

//#define BUILD_STRING ""

#if defined(WIN32) || defined(__WIN32__)
#define BUILD_STRING_PLATFORM "win32"
#else
#define BUILD_STRING_PLATFORM "unknown"
#endif

#if defined(LINK_STATIC_RUNTIME)
#define BUILD_STRING_LINKAGE "-static"
#else
#define BUILD_STRING_LINKAGE ""
#endif

#define VERSION_MINOR_STRING "1"
#define VERSION_MAJOR_STRING "1"
#define VERSION_REV_STRING "0"

#define BUILD_STRING_VERSION ("-" (VERSION_MINOR_STRING) "." (VERSION_MAJOR_STRING) "." (VERSION_REV_STRING))

#define BUILD_STRING ((BUILD_STRING_PLATFORM) (BUILD_STRING_LINKAGE) (BUILD_STRING_VERSION));
