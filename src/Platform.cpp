#include "Platform.h"

#ifdef WIN32
#  include <windows.h>
#  include <shlobj.h>
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
#  include <unistd.h>
#  include <stdlib.h>
#  include <iostream>
#  include <fstream>
#endif

#ifdef USE_QEXT_PLATFORM_H
#  include "qt-gui/QExtPlatform.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "util/Filesystem.h"


using namespace std;

namespace platform {
	bool haveConsole =
#ifdef NDEBUG
		false;
#else
		true;
#endif

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

	void activateWindow(const std::string& wid)
	{
#ifdef WIN32
		// wid is a string created by boost::lexical_cast<std::string>(hwnd)
		// as HWND is a pointer type this will be a hex value
		// so we'll need to do some manual parsing
		uintptr_t val = 0;
		for (size_t i = 0; i < wid.size(); ++i) {
			char c = wid[i];
			int cval = 0;
			if (c >= '0' && c <= '9') cval = c - '0';
			if (c >= 'a' && c <= 'f') cval = c - 'a' + 10;
			if (c >= 'A' && c <= 'F') cval = c - 'A' + 10;
			val <<= 4;
			val |= cval;
		}
		HWND wh = (HWND)val;
		SetForegroundWindow(wh);
#endif
	}

	int RunCommand(const string& cmd, const vector<string>* args, const string& workdir, int flags)
	{
#ifdef WIN32
		string command;

		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;

		command += "\"";
		command += cmd;
		command += "\"";

		// TODO args
		if (args) {
			for (unsigned int i = 0; i < args->size(); ++i) {
				command += " \"";
				command += (*args)[i];
				command += "\"";
			}
		}

		int res = -1;
		if (CreateProcess(
				NULL,
				TEXT((char*)command.c_str()),
				NULL,
				NULL,
				FALSE,
				(flags & RF_NO_WINDOW)   ? CREATE_NO_WINDOW   : 0 |
				(flags & RF_NEW_CONSOLE) ? CREATE_NEW_CONSOLE : 0 ,
				NULL, (workdir=="") ? NULL : TEXT(workdir.c_str()), &si, &pi))
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

		LPSTR Path = new TCHAR[MAX_PATH];
		if (SHGetPathFromIDList(pidlist, Path))
			retval = Path;

		delete[] Path;
		ILFree(pidlist);
		return retval;
	}
#endif

	spathlist getSettingsPathList() {
		spathlist result;
		ext::filesystem::path currentdir(boost::filesystem::current_path<ext::filesystem::path>());
		result.push_back(spathinfo("Current Directory"      , currentdir / "uftt.dat"));
		//if (!ApplicationPath.empty())
		//	result.push_back(spathinfo("Executable Directory", ApplicationPath.branch_path() / "uftt.dat"));
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
	string _getenv(string s) {
		char* r = getenv(s.c_str());
		return (r == NULL) ? string() : string(r);
	}

	string scan_xdg_user_dirs(string dirname) {
		string result;
		ext::filesystem::path xdgConfigHome(string(_getenv("XDG_CONFIG_HOME")));
		if(!ext::filesystem::exists(xdgConfigHome))
			xdgConfigHome = ext::filesystem::path(string(_getenv("HOME")) + "/.config");
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
							result = string(_getenv("HOME")) + result.substr(5);
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

		p = ext::filesystem::path(_getenv("DESKTOP"));
		if(ext::filesystem::exists(p))
			return p;

		p = ext::filesystem::path(_getenv("HOME")) / "Desktop";
		if(ext::filesystem::exists(p))
			return p;

		p = ext::filesystem::path(_getenv("HOME"));
		if(ext::filesystem::exists(p))
			return p;

		/* Should never happen */
		char dirname_template[] = "uftt-XXXXXX";
		char* dirname = mkdtemp(dirname_template);
		p = ext::filesystem::path(dirname);
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

#ifdef WIN32
	std::string convertTStringToUTF8(const char* tstring)
	{
		return convertLocaleToUTF8(tstring);
	}

	std::string convertTStringToUTF8(const wchar_t* tstring)
	{
		return convertUTF16ToUTF8(tstring);
	}
#endif

	std::string getUserName()
	{
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
		wchar_t tname[256];
		DWORD len = 256;
		if (GetUserNameW(tname, &len))
			return convertTStringToUTF8(tname);
#elif defined(WIN32) && defined(_WIN32_WINDOWS)
		char tname[256];
		DWORD len = 256;
		if (GetUserNameA(tname, &len))
			return convertTStringToUTF8(tname);
#endif
		char* name = NULL;
		if (!name) name = getenv("USER");
		if (!name) name = getenv("USERNAME");
		if (!name) return "uftt-user";
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
		WCHAR wcs[MAX_PATH];

		// Retrieve object pointers and convert source path
		success = success && SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&isl));
		success = success && SUCCEEDED(isl->QueryInterface(IID_IPersistFile, (LPVOID*)&ipf));
		success = success && SUCCEEDED(MultiByteToWideChar(CP_ACP, 0, TEXT(source.native_file_string().c_str()), -1, wcs, MAX_PATH));

		if (success) {
			// load previous shortcut from disk
			success = SUCCEEDED(ipf->Load(wcs, STGM_READ));
			if (success) {
				// check if existing shortcut points to our target
				success = success && SUCCEEDED(isl->Resolve(NULL, SLR_NO_UI));

				WIN32_FIND_DATA wfd;
				char szGotPath[MAX_PATH];

				success = success && SUCCEEDED(isl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA*)&wfd, 0));
				success = success && boost::iequals(ext::filesystem::path(szGotPath).string(), target.string());
			}

			if (!success) {
				// save shortcut with our target (either failed to load or it did not point to our target)
				success = true;
				success = success && SUCCEEDED(isl->SetPath(TEXT(target.native_file_string().c_str())));
				success = success && SUCCEEDED(isl->SetWorkingDirectory(TEXT(target.branch_path().native_file_string().c_str())));
				success = success && SUCCEEDED(isl->SetDescription(TEXT(description.c_str())));
				success = success && SUCCEEDED(ipf->Save(wcs, TRUE));
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
			return boost::filesystem::remove(source);
		}
	}

	ext::filesystem::path getApplicationPath()
	{
		// currently only used on windows
#ifdef WIN32
		// test for unicode support?
		char module_name[MAX_PATH];
		GetModuleFileNameA(0, module_name, MAX_PATH);
		ext::filesystem::path path = module_name;
		if (!ext::filesystem::exists(path)) path = "";
		return path;
#endif
		return ext::filesystem::path();
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
		if (!enabled) ret = boost::filesystem::remove(menudir) && ret;
		return ret;
#endif
		return false;
	}

	std::wstring convertUTF8ToUTF16(const std::string& src)
	{
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
		WCHAR wcs[MAX_PATH];
		if (!SUCCEEDED(MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, wcs, MAX_PATH)))
			throw std::runtime_error("convertUTF8ToUTF16: MultiByteToWideChar Failed");
		return std::wstring(wcs);
#else
		throw std::runtime_error("convertUTF8ToUTF16: Not implemented");
#endif
	}

	std::string convertUTF16ToUTF8(const std::wstring& src)
	{
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
		char res[MAX_PATH];
		if (!SUCCEEDED(WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, res, MAX_PATH, NULL, NULL)))
			throw std::runtime_error("convertUTF8ToUTF16: WideCharToMultiByte Failed");
		return std::string(res);
#else
		throw std::runtime_error("convertUTF8ToUTF16: Not implemented");
#endif
	}

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

} // namespace platform
