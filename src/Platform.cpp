#include "Platform.h"

#ifdef WIN32
#  ifndef _WIN32_WINDOWS
#    define UNICODE
#  endif
#  include <windows.h>
#  include <shlobj.h>
#  include <shobjidl.h>
#  include <stdio.h>
#  include <fcntl.h>
#  include <io.h>
#  include <iostream>
#  include <fstream>

#  ifndef PIDLIST_ABSOLUTE
#    define PIDLIST_ABSOLUTE LPITEMIDLIST
#  endif

#  ifndef CSIDL_MYDOCUMENTS
#    define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
#  endif

#else
#  include <signal.h>
#  include <unistd.h>
#  include <stdlib.h>
#  include <iostream>
#  include <fstream>
#endif

#ifdef USE_QEXT_PLATFORM_H
#  include "qt-gui/QExtPlatform.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "util/Filesystem.h"

using namespace std;

namespace platform {
	#ifdef WIN32
		#ifdef _WIN32_WINDOWS
			typedef std::string tstring;

			std::string convertTStringToUTF8(const char* tcs)
			{
				return convertLocaleToUTF8(tcs);
			}

			std::string convertUTF8ToTString(const std::string& in)
			{
				return convertUTF8ToLocale(in);
			}
		#else
			typedef std::wstring tstring;

			std::string convertTStringToUTF8(const wchar_t* tcs)
			{
				return convertUTF16ToUTF8(tcs);
			}

			std::wstring convertUTF8ToTString(const std::string& in)
			{
				return convertUTF8ToUTF16(in);
			}
		#endif
	#endif

	bool haveConsole = false;

	// returns whether we have a console attached to stdout
	bool hasConsole()
	{
		return haveConsole;
	}

	// allocates a new console for stdout
	bool newConsole()
	{
		if (hasConsole()) return false;
#ifdef WIN32
		int hConHandle;
		intptr_t lStdHandle;
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		FILE *fp;
		// allocate a console for this app
		AllocConsole();
		// set the screen buffer to be big enough to let us scroll text
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
		coninfo.dwSize.Y = 500;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
		// redirect unbuffered STDOUT to the console
		lStdHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
		if (!lStdHandle) return false;
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		if (hConHandle == -1) return false;
		fp = _fdopen( hConHandle, "w" );
		if (!fp) return false;
		*stdout = *fp;
		setvbuf( stdout, NULL, _IONBF, 0 );
		// redirect unbuffered STDIN to the console
		lStdHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);
		if (!lStdHandle) return false;
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		if (hConHandle == -1) return false;
		fp = _fdopen( hConHandle, "r" );
		if (!fp) return false;
		*stdin = *fp;
		setvbuf( stdin, NULL, _IONBF, 0 );
		// redirect unbuffered STDERR to the console
		lStdHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);
		if (!lStdHandle) return false;
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		if (hConHandle == -1) return false;
		fp = _fdopen( hConHandle, "w" );
		if (!fp) return false;
		*stderr = *fp;
		setvbuf( stderr, NULL, _IONBF, 0 );
		// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
		// point to console as well
		ios::sync_with_stdio();
		haveConsole = true;
		return true;
#else
		return false;
#endif
	}

	// frees the stdout console
	bool freeConsole()
	{
		if (!hasConsole()) return false;
#ifdef WIN32
		::FreeConsole();
		haveConsole = false;
		return true;
#else
		return false;
#endif
	}

	void exit(unsigned int ret)
	{
#ifdef WIN32
		::ExitProcess(ret);
#endif
	}

	string getenv(const string& s) {
		char* r = ::getenv(s.c_str());
		return (r == NULL) ? string() : string(r);
	}

#ifdef WIN32
	HWND parseHWND(const std::string& hwnd)
	{
		// wid is a string created by boost::lexical_cast<std::string>(hwnd)
		// as HWND is a pointer type this will be a hex value
		// so we'll need to do some manual parsing
		uintptr_t val = 0;
		for (size_t i = 0; i < hwnd.size(); ++i) {
			char c = hwnd[i];
			int cval = -1;
			if (c >= '0' && c <= '9') cval = c - '0';
			if (c >= 'a' && c <= 'f') cval = c - 'a' + 10;
			if (c >= 'A' && c <= 'F') cval = c - 'A' + 10;
			if (cval == -1) return (HWND)0;
			val <<= 4;
			val |= cval;
		}
		return (HWND)val;
	}
#endif

	void activateWindow(const std::string& wid)
	{
#ifdef WIN32
		SetForegroundWindow(parseHWND(wid));
#endif
	}

	int RunCommand(const string& cmd, const vector<string>* args, const string& workdir, int flags)
	{
#ifdef WIN32
		string command;
		bool quote = !(flags&RF_NOQUOTING);

		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;

		if (quote) command += '\"';
		command += cmd;
		if (quote) command += '\"';

		// TODO args
		if (args) {
			for (unsigned int i = 0; i < args->size(); ++i) {
				command += ' ';
				if (quote) command += '\"';
				command += (*args)[i];
				if (quote) command += '\"';
			}
		}

		command += '\x00';
		tstring ncmd = convertUTF8ToTString(command);
		int res = -1;
		if (CreateProcess(
				NULL,
				&ncmd[0],
				NULL,
				NULL,
				FALSE,
				(flags & RF_NO_WINDOW)   ? CREATE_NO_WINDOW   : 0 |
				(flags & RF_NEW_CONSOLE) ? CREATE_NEW_CONSOLE : 0 ,
				NULL, workdir.empty() ? NULL : convertUTF8ToTString(workdir).c_str(), &si, &pi))
		{
			if (flags & RF_WAIT_FOR_EXIT) {
				DWORD exitcode;
				WaitForSingleObject(pi.hProcess, INFINITE);
				if (GetExitCodeProcess(pi.hProcess, &exitcode))
					res = exitcode;
			} else
				res = 0;
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		return res;
#else
		// TODO: ? is it needed, probably only used by autoupdate
		return -1;
#endif
	}

#ifdef WIN32
	ext::filesystem::path getFolderLocation(int nFolder)
	{
		ext::filesystem::path retval;
		PIDLIST_ABSOLUTE pidlist;
		HRESULT res = SHGetSpecialFolderLocation(
			NULL,
			nFolder,
			//NULL,
			//0,
			&pidlist
		);
		if (res != S_OK) return retval;

		TCHAR Path[MAX_PATH];
		if (SHGetPathFromIDList(pidlist, Path))
			retval = convertTStringToUTF8(Path);

		ILFree(pidlist);
		return retval;
	}
#endif

	spathlist getSettingsPathList() {
		spathlist result;
		result.push_back(spathinfo("Current Directory"      , ext::filesystem::current_path() / "uftt.dat"));
		ext::filesystem::path apppath = getApplicationPath();
		if (!apppath.empty())
			result.push_back(spathinfo("Executable Directory", apppath.parent_path() / "uftt.dat"));
#ifdef WIN32
		result.push_back(spathinfo("User Documents"         , getFolderLocation(CSIDL_MYDOCUMENTS)    / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("Common Application Data", getFolderLocation(CSIDL_COMMON_APPDATA) / "UFTT" / "uftt.dat"));
#else
		result.push_back(spathinfo("Home Directory"         , ext::filesystem::path(getenv("HOME")) / ".uftt" / "uftt.dat"));
#endif
		return result;
	}

	spathinfo getSettingsPathDefault() {
#ifdef WIN32
		return spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat");
#else
		return spathinfo("Home Directory"         , ext::filesystem::path(getenv("HOME")) / ".uftt" / "uftt.dat");
#endif
	}

#ifndef WIN32
	string scan_xdg_user_dirs(string dirname) {
		string result;
		ext::filesystem::path xdgConfigHome(getenv("XDG_CONFIG_HOME"));
		if(!ext::filesystem::exists(xdgConfigHome))
			xdgConfigHome = ext::filesystem::path(getenv("HOME") + "/.config");
		if(ext::filesystem::exists(xdgConfigHome) && boost::filesystem::is_directory(xdgConfigHome)) {
			ext::filesystem::path file(xdgConfigHome / "user-dirs.dirs");
			if(boost::filesystem::is_regular(file)) {
				ifstream ifs(file.string().c_str(), ios_base::in | ios_base::binary);
				string line;
				string pattern = string() + "XDG_" + dirname + "_DIR=";
				while(!ifs.eof()) {
					getline(ifs, line);
					if(line.find(pattern) == 0) { //NOTE: Really strict, like Qt
						result = line.substr(pattern.size());
						if((result.find('"') == 0) && (result.rfind('"') == result.size() - 1))
							result = result.substr(1, result.size() - 2);
						if(result.find("$HOME") == 0) // FIXME: Hax! (like Qt)
							result = getenv("HOME") + result.substr(5);
					}
				}
			}
		}
		return result;
	}
#endif

	ext::filesystem::path getDownloadPathDefault() {
#ifdef WIN32
		return getFolderLocation(CSIDL_DESKTOP);
#else
		ext::filesystem::path p;

		p = ext::filesystem::path(scan_xdg_user_dirs("DESKTOP"));
		if(ext::filesystem::exists(p))
			return p;

		p = ext::filesystem::path(getenv("DESKTOP"));
		if(ext::filesystem::exists(p))
			return p;

		p = ext::filesystem::path(getenv("HOME")) / "Desktop";
		if(ext::filesystem::exists(p))
			return p;

		p = ext::filesystem::path(getenv("HOME"));
		if(ext::filesystem::exists(p))
			return p;

#ifndef ANDROID
		/* Should never happen */
		char dirname_template[] = "uftt-XXXXXX";
		char* dirname = mkdtemp(dirname_template);
		p = ext::filesystem::path(dirname);
#endif
		return p;
#endif
	}

	void msSleep(int ms)
	{
#ifdef WIN32
		Sleep(ms);
#else
		usleep(ms*1000);
#endif
	}

	bool fseek64a(FILE* fd, uint64 offset)
	{
		// see http://code.google.com/p/pspstacklesspython/source/browse/vendor/stackless-25-maint/Objects/fileobject.c for hints
		// maybe use their _portable_fseek function?
#ifdef WIN32
		int64 off = offset;
		return (fsetpos(fd, &off) == 0);
#else
		// Linux
		return (fseeko(fd, offset, SEEK_SET) == 0);
#endif
	}

	std::string getUserName()
	{
		std::string name;
#if defined(WIN32)
		if (name.empty()) {
			TCHAR tname[256];
			DWORD len = 256;
			if (GetUserName(tname, &len))
				name =  convertTStringToUTF8(tname);
		}
#endif
		if (name.empty()) name = getenv("USER");
		if (name.empty()) name = getenv("USERNAME");
		if (name.empty()) name = "uftt-user";
		return name;
	}

	bool createLink(const std::string& description, const ext::filesystem::path& source, const ext::filesystem::path& target)
	{
		// currently only used on windows
#ifdef WIN32
		CoInitialize(NULL);

		bool success = true;
		IShellLink* isl = NULL;
		IPersistFile* ipf = NULL;
		tstring nsource = convertUTF8ToTString(source.native_file_string());

		// Retrieve object pointers
		success = success && SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&isl));
		success = success && SUCCEEDED(isl->QueryInterface(IID_IPersistFile, (LPVOID*)&ipf));

		if (success) {
			// load previous shortcut from disk
			success = SUCCEEDED(ipf->Load(nsource.c_str(), STGM_READ));
			if (success) {
				// check if existing shortcut points to our target
				success = success && SUCCEEDED(isl->Resolve(NULL, SLR_NO_UI));

				WIN32_FIND_DATA wfd;
				TCHAR szGotPath[MAX_PATH];

				success = success && SUCCEEDED(isl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA*)&wfd, 0));
				success = success && boost::iequals(ext::filesystem::path(convertTStringToUTF8(szGotPath)).string(), target.string());
			}

			if (!success) {
				// save shortcut with our target (either failed to load or it did not point to our target)
				success = true;
				success = success && SUCCEEDED(isl->SetPath(convertUTF8ToTString(target.native_file_string()).c_str()));
				success = success && SUCCEEDED(isl->SetWorkingDirectory(convertUTF8ToTString(target.parent_path().native_file_string()).c_str()));
				success = success && SUCCEEDED(isl->SetDescription(convertUTF8ToTString(description).c_str()));
				success = success && SUCCEEDED(ipf->Save(nsource.c_str(), TRUE));
			}
		}
		if (ipf) ipf->Release();
		if (isl) isl->Release();
		CoUninitialize();
		return success;
#endif
		return false;
	}

	bool createRemoveLink(bool create, std::string name, const std::string& description, const ext::filesystem::path& sourcedir, const ext::filesystem::path& target)
	{
		if (!ext::filesystem::exists(sourcedir)) return false;
#ifdef WIN32
		name += ".lnk";
#endif
		ext::filesystem::path source = sourcedir / name;
		if (create) {
			if (!ext::filesystem::exists(target)) return false;
			return createLink(description, source, target);
		} else {
			return ext::filesystem::remove(source);
		}
	}

	ext::filesystem::path getApplicationPath()
	{
		ext::filesystem::path path;
#ifdef WIN32
		TCHAR module_name[MAX_PATH];
		DWORD retsize = GetModuleFileName(0, module_name, MAX_PATH);
		if (retsize != 0 && retsize != MAX_PATH)
			path = convertTStringToUTF8(module_name);
#else
		// Some hints to implement this:
		//   http://www.linuxquestions.org/questions/programming-9/unix-function-call-to-get-executables-path-470000/
		//   http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
		// However on unix the need for this is usually considered a design flaw...
#endif
		if (!path.empty() && !ext::filesystem::exists(path))
			path = "";
		return path;
	}

	bool setSendToUFTTEnabled(bool enabled)
	{
		// only makes sense for windows?
#ifdef WIN32
		return createRemoveLink(enabled, "UFTT", "Ultimate File Transfer Tool", getFolderLocation(CSIDL_SENDTO), getApplicationPath());
#endif
		return false;
	}

	bool setDesktopShortcutEnabled(bool enabled)
	{
#ifdef WIN32
		return createRemoveLink(enabled, "UFTT", "Ultimate File Transfer Tool", getFolderLocation(CSIDL_DESKTOP), getApplicationPath());
#endif
		return false;
	}

	bool setQuicklaunchShortcutEnabled(bool enabled)
	{
#ifdef WIN32
		return createRemoveLink(enabled, "UFTT", "Ultimate File Transfer Tool", getFolderLocation(CSIDL_APPDATA) / "Microsoft/Internet Explorer/Quick Launch", getApplicationPath());
#endif
		return false;
	}

	bool setStartmenuGroupEnabled(bool enabled)
	{
#ifdef WIN32
		ext::filesystem::path menudir = getFolderLocation(CSIDL_PROGRAMS);
		if (!ext::filesystem::exists(menudir)) return false;
		menudir /= "UFTT";
		if (enabled && !boost::filesystem::create_directory(menudir)) return false;
		bool ret = createRemoveLink(enabled, "UFTT", "Ultimate File Transfer Tool", menudir, getApplicationPath());
		if (!enabled) ret = ext::filesystem::remove(menudir) && ret;
		return ret;
#endif
		return false;
	}

#if defined(WIN32) && !defined(_WIN32_WINDOWS)
	std::wstring convertUTF8ToUTF16(const std::string& src)
	{
		if (src.empty()) return std::wstring();
		std::vector<WCHAR> wcs(src.size()); // worst case: 1xUTF8 -> 1xUTF16
		int rsize = MultiByteToWideChar(CP_UTF8, 0, src.data(), src.size(), &wcs[0], wcs.size());
		if (rsize == 0)
			throw std::runtime_error("convertUTF8ToUTF16: MultiByteToWideChar Failed");
		return std::wstring(wcs.begin(), wcs.begin()+rsize);
	}

	std::string convertUTF16ToUTF8(const std::wstring& src)
	{
		if (src.empty()) return std::string();
		std::vector<char> res(src.size()*3); // worst case: 1xUTF16 -> 3xUTF8
		int rsize = WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(), &res[0], res.size(), NULL, NULL);
		if (rsize == 0)
			throw std::runtime_error("convertUTF8ToUTF16: WideCharToMultiByte Failed");
		return std::string(res.begin(), res.begin()+rsize);
	}
#endif

	std::string convertUTF8ToLocale(const std::string& src)
	{
#ifdef USE_QEXT_PLATFORM_H
		return qext::platform::convertUTF8ToLocale(src);
#else
		throw std::runtime_error("convertUTF8ToLocale: Not implemented");
#endif
	}

	std::string convertLocaleToUTF8(const std::string& src)
	{
#ifdef USE_QEXT_PLATFORM_H
		return qext::platform::convertLocaleToUTF8(src);
#else
		throw std::runtime_error("convertLocaleToUTF8: Not implemented");
#endif
	}

	std::string makeValidUTF8(const std::string& src)
	{
		const int BINARY_1000_0000 = 0x80;
		const int BINARY_1100_0000 = 0xC0;
		const int BINARY_1110_0000 = 0xE0;
		const int BINARY_1111_0000 = 0xF0;
		const int BINARY_1111_1000 = 0xF8;
		const int BINARY_0011_1111 = 0x3F;
		const int BINARY_0001_1111 = 0x1F;
		const int BINARY_0000_1111 = 0x0F;
		const int BINARY_0000_0111 = 0x07;

		std::string ret;
		size_t i = 0;
		unsigned char c;
		while(i < src.size()) {
			size_t start = i;
			c = src[i];
			if((c & BINARY_1000_0000) != 0) { // Not US-ASCII
				uint32 code_point = 0;
				int expected = 0;
				if((c & BINARY_1110_0000) == BINARY_1100_0000) { // start of a 2-byte sequence
					code_point = c & BINARY_0001_1111;
					expected = 2;
				} else
				if((c & BINARY_1111_0000) == BINARY_1110_0000) { // start of a 3-byte sequence
					code_point = c & BINARY_0000_1111;
					expected = 3;
				} else
				if((c & BINARY_1111_1000) == BINARY_1111_0000) { // start of a 4-byte sequence
					code_point = c & BINARY_0000_0111;
					expected = 4;
				}
				else {
					expected = 0;
				}
				++i;
				while(i < src.size() && ((src[i] & BINARY_1100_0000) == BINARY_1000_0000)) {
					code_point = code_point << 6 | (src[i] & BINARY_0011_1111);
					++i;
					if(expected>0)
						--expected;
				}
				if((expected != 1) || (code_point < 128) || (code_point > 0x10ffff)) { // invalid code_point
					ret += "\xC2\xBF"; // U+00BF (upside-down question mark)
				}
				else {
					ret += src.substr(start, i-start);
				}
			}
			else {
				if (src[i] != 0) // clean up autoupdate shares
					ret += src[i];
				++i;
			}
		}
		return ret;
	}

	std::vector<std::string> getUTF8CommandLine(int argc, char **argv)
	{
		std::vector<std::string> res;
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
		// windows nt has api to get unicode command line
		int nArgs = 0;
		LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
		for (int i = 0; i < nArgs; ++i)
			res.push_back(platform::convertUTF16ToUTF8(szArglist[i]));
		LocalFree(szArglist);
#elif defined(WIN32) && defined(_WIN32_WINDOWS)
		// windows 9x has argv[i] in the current locale
		for (int i = 0; i < argc; ++i)
			res.push_back(convertLocaleToUTF8(argv[i]));
#else
		// assumes argv[i] is already UTF8... (usually true in linux)
		for (int i = 0; i < argc; ++i)
			res.push_back(argv[i]);
#endif
		return res;
	}

	struct TaskbarProgress::pimpl_t {
#if defined(WIN32) && defined(__ITaskbarList3_INTERFACE_DEFINED__)
		HWND hwnd;
		ITaskbarList3* itbl3;
#endif
	};

	TaskbarProgress::TaskbarProgress(const std::string& wid)
	{
		pimpl = new pimpl_t();
#if defined(WIN32) && defined(__ITaskbarList3_INTERFACE_DEFINED__)
		pimpl->hwnd = parseHWND(wid);
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&pimpl->itbl3);
#endif
	}

	TaskbarProgress::~TaskbarProgress()
	{
#if defined(WIN32) && defined(__ITaskbarList3_INTERFACE_DEFINED__)
		if (pimpl->itbl3) pimpl->itbl3->Release();
#endif
		delete pimpl;
	}

	void TaskbarProgress::setValue(uint64 current, uint64 total)
	{
#if defined(WIN32) && defined(__ITaskbarList3_INTERFACE_DEFINED__)
		if (pimpl->itbl3) {
			pimpl->itbl3->SetProgressValue(pimpl->hwnd, current, total);
			pimpl->itbl3->SetProgressState(pimpl->hwnd, TBPF_NORMAL);
		}
#endif
	}

	void TaskbarProgress::setStateError()
	{
#if defined(WIN32) && defined(__ITaskbarList3_INTERFACE_DEFINED__)
		if (pimpl->itbl3) pimpl->itbl3->SetProgressState(pimpl->hwnd, TBPF_ERROR);
#endif
	}

	bool openContainingFolder(const ext::filesystem::path& itempath)
	{
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
		ITEMIDLIST *pidl = ILCreateFromPath(convertUTF8ToTString(itempath.native_file_string()).c_str());
		if (pidl) {
			HRESULT hres = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
			ILFree(pidl);
			if (hres == S_OK)
				return true;
			else
				std::cout << "openContainingFolder1: " << hres << "\n";
		}
#endif
#ifdef WIN32
		std::vector<std::string> args;
		args.push_back(std::string() + "/select," + itempath.native_file_string());
		// explorer.exe /select doesn't need the filename quoted, and doing so actually breaks it
		int ret = RunCommand("explorer.exe", &args, "", RF_WAIT_FOR_EXIT|RF_NOQUOTING);
		// seems to always return 1, don't know why, maybe should just always return true here?
		if (ret == 1)
			return true;
		else
			std::cout << "openContainingFolder2: " << ret << "\n";
#endif
		return false;
	}

	std::string getCurrentPID()
	{
#ifdef WIN32
		DWORD pid = GetCurrentProcessId();
		return boost::lexical_cast<std::string>(pid);
#else
		pid_t pid = getpid();
		return boost::lexical_cast<std::string>(pid);
#endif
	}

	struct PHANDLE {
#ifdef WIN32
		HANDLE phandle;
#else
		pid_t pid;
#endif
	};

	PHANDLE* getProcessHandle(const std::string& pid)
	{
		PHANDLE* ret = new PHANDLE();
#ifdef WIN32
		ret->phandle = OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION, false, boost::lexical_cast<DWORD>(pid));
#else
		ret->pid = boost::lexical_cast<pid_t>(pid);
#endif
		return ret;
	}

	int waitForProcessExit(PHANDLE* handle)
	{
		int res = -1;
#ifdef WIN32
		DWORD exitcode;
		WaitForSingleObject(handle->phandle, INFINITE);
		if (GetExitCodeProcess(handle->phandle, &exitcode))
			res = exitcode;
#else
		for (int i = 0; i < 5*100; ++i) {
			if (kill(handle->pid, 0) == -1 && errno == ESRCH) {
				res = 0; // no way to get actual exit code of non-child processes
				break;
			}
			msSleep(10); // 100/sec
		}
#endif
		return res;
	}

	void freeProcessHandle(PHANDLE* handle)
	{
#ifdef WIN32
		CloseHandle(handle->phandle);
#endif
		delete handle;
	}
} // namespace platform
