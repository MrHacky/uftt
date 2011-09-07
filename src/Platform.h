#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>

#include "util/Filesystem.h"

#include "Types.h"
/* file for platform compatibility wrappers */

namespace platform {
	// returns whether we have a console attached to stdout
	bool hasConsole();

	// allocates a new console for stdout
	bool newConsole();

	// frees the stdout console
	bool freeConsole();

	// exits the current process
	void exit(unsigned int ret);

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

	/**
	 * Converts a string which may or may not be in UTF8 to a valid UTF8
	 * string by replacing invalid UTF8 codes with U+00BF.
	 * @param src is the input string.
	 * @return the input string, with all invalid UTF8 encodings replaced by U+00BF.
	 * @note This function may alter the length (number of bytes) of the input string.
	 */
	std::string makeValidUTF8(const std::string& src);

	/**
	 * Get command line in UTF8
	 * @param argc Use argc as was passed to main()
	 * @param argv Use argv as was passed to main()
	 * @return A vector containing UTF8 strings, which correspond to the command line
	 */
	std::vector<std::string> getUTF8CommandLine(int argc, char **argv);

	/**
	 * Get path of the drag target. Call after drag is completed.
	 * @return Drag target path if successfull, empty path if failed.
	 * @note Relies on current mouse cursor position to locate drop target.
	 * @note Only works on windows, and probably not even 100% then.
	 */
	ext::filesystem::path getDragTargetPath();

	///> Wrapper for taskbar progress interface in windows 7
	class TaskbarProgress {
		public:
			TaskbarProgress(const std::string& wid);
			~TaskbarProgress();

			void setValue(uint64 current, uint64 total);
			void setStateError();

		private:
			struct pimpl_t;
			pimpl_t* pimpl;
	};
} // namespace platform

#endif//PLATFORM_H
