#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "Types.h"
/* file for platform compatibility wrappers */

namespace platform {
	// returns whether we have a console attached to stdout
	bool hasConsole();

	// allocates a new console for stdout
	bool newConsole();

	// frees the stdout console
	bool freeConsole();

	void msSleep(int ms);

	bool fseek64a(FILE* fd, uint64 offset);

	enum {
		RF_NEW_CONSOLE   = 1 << 0,
		RF_WAIT_FOR_EXIT = 1 << 1,
		RF_NO_WINDOW     = 1 << 2,
	};

	int RunCommand(const std::string& cmd, const std::vector<std::string>* args, const std::string& workdir, int flags);

	void setApplicationPath(const boost::filesystem::path& path);

	typedef std::pair<std::string, boost::filesystem::path> spathinfo;
	typedef std::vector<spathinfo> spathlist;

	spathlist getSettingsPathList();
	spathinfo getSettingsPathDefault();

	std::string getUserName();
} // namespace platform

#endif//PLATFORM_H
