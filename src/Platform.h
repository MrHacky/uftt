#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>

#include "util/filesystem.h"

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

	void activateWindow(const std::string& wid);

	ext::filesystem::path getApplicationPath();

	bool setSendToUFTTEnabled(bool enabled);
	bool setDesktopShortcutEnabled(bool enabled);
	bool setQuicklaunchShortcutEnabled(bool enabled);
	bool setStartmenuGroupEnabled(bool enabled);

	typedef std::pair<std::string, ext::filesystem::path> spathinfo;
	typedef std::vector<spathinfo> spathlist;

	spathlist getSettingsPathList();
	spathinfo getSettingsPathDefault();
	ext::filesystem::path getDownloadPathDefault();

	std::string getUserName();

	std::wstring convertUTF8ToUTF16(const std::string& src);
	std::string convertUTF16ToUTF8(const std::wstring& src);
	std::string convertUTF8ToLocale(const std::string& src);
	std::string convertLocaleToUTF8(const std::string& src);
} // namespace platform

#endif//PLATFORM_H
