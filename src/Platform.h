#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

/* file for platform compatibility wrappers */

namespace platform {
	// returns whether we have a console attached to stdout
	bool hasConsole();

	// allocates a new console for stdout
	bool newConsole();

	// frees the stdout console
	bool freeConsole();

	void msSleep(int ms);

	enum {
		RF_NEW_CONSOLE   = 1 << 0,
		RF_WAIT_FOR_EXIT = 1 << 1,
	};

	int RunCommand(const std::string& cmd, const std::vector<std::string>* args, const std::string& workdir, int flags);

	typedef std::pair<std::string, boost::filesystem::path> spathinfo;
	typedef std::vector<spathinfo> spathlist;

	spathlist getSettingPathList();
} // namespace platform

#endif//PLATFORM_H
