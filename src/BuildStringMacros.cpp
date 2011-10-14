char build_string_macro_date[] = __DATE__;
char build_string_macro_time[] = __TIME__;

#include "BuildVersion.h"

char build_string_version_major[] = VERSION_MAJOR_STRING;
char build_string_version_minor[] = VERSION_MINOR_STRING;
char build_string_version_patch[] = VERSION_PATCH_STRING;
char build_string_version_btype[] = VERSION_BTYPE_STRING;
char build_string_version_extra[] = VERSION_EXTRA_STRING;

#include "SvnRevision.h"

char build_string_version_svnrev[] = SVN_REVISION_STRING;
