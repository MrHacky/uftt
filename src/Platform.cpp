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
#  include "util/Filesystem.h"
#endif


#include <boost/filesystem.hpp>

using namespace std;

namespace {
	boost::filesystem::path ApplicationPath;
}

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
	boost::filesystem::path getFolderLocation(int nFolder)
	{
		boost::filesystem::path retval;
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

	void setApplicationPath(const boost::filesystem::path& path)
	{
		ApplicationPath = path;
	}

	spathlist getSettingsPathList() {
		spathlist result;
		boost::filesystem::path currentdir(boost::filesystem::current_path<boost::filesystem::path>());
		result.push_back(spathinfo("Current Directory"      , currentdir / "uftt.dat"));
		if (!ApplicationPath.empty())
			result.push_back(spathinfo("Executable Directory", ApplicationPath.branch_path() / "uftt.dat"));
#ifdef WIN32
		result.push_back(spathinfo("User Documents"         , getFolderLocation(CSIDL_MYDOCUMENTS)    / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("Common Application Data", getFolderLocation(CSIDL_COMMON_APPDATA) / "UFTT" / "uftt.dat"));
#else
		result.push_back(spathinfo("Home Directory"         , boost::filesystem::path(getenv("HOME")) / ".uftt" / "uftt.dat"));
#endif
		return result;
	}

	spathinfo getSettingsPathDefault() {
#ifdef WIN32
		return spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat");
#else
		return spathinfo("Home Directory"         , boost::filesystem::path(getenv("HOME")) / ".uftt" / "uftt.dat");
#endif
	}

#ifndef WIN32
	string _getenv(string s) {
		char* r = getenv(s.c_str());
		return (r == NULL) ? string() : string(r);
	}

	string scan_xdg_user_dirs(string dirname) {
		string result;
		boost::filesystem::path xdgConfigHome(string(_getenv("XDG_CONFIG_HOME")));
		if(!boost::filesystem::exists(xdgConfigHome))
			xdgConfigHome = boost::filesystem::path(string(_getenv("HOME")) + "/.config");
		if(ext::filesystem::exists(xdgConfigHome) && boost::filesystem::is_directory(xdgConfigHome)) {
			boost::filesystem::path file(xdgConfigHome / "user-dirs.dirs");
			if(boost::filesystem::is_regular(file)) {
				ifstream ifs(file.string().c_str(), ios_base::in | ios_base::binary);
				string line;
				string pattern = string() + "XDG_" + dirname + "_DIR=";
				while(!ifs.eof()) {
					getline(ifs, line);
					if(line.find(pattern) == 0) { //NOTE: Really strict, like Qt
						result = line.substr(pattern.size(), line.size() - pattern.size());
						if((result.find('"') == 0) && (result.rfind('"') == result.size() - 1))
							result = result.substr(1, result.size() - 2);
						if(result.find("$HOME") == 0) // FIXME: Hax! (like Qt)
							result = string(_getenv("HOME")) + result.substr(5, result.size() - 5);
					}
				}
			}
		}
		return result;
	}
#endif

	boost::filesystem::path getDownloadPathDefault() {
#ifdef WIN32
		return getFolderLocation(CSIDL_DESKTOP);
#else
		boost::filesystem::path p;

		p = boost::filesystem::path(scan_xdg_user_dirs("DESKTOP"));
		if(ext::filesystem::exists(p))
			return p;
		
		p = boost::filesystem::path(_getenv("DESKTOP"));
		if(ext::filesystem::exists(p))
			return p;

		p = boost::filesystem::path(_getenv("HOME")) / "Desktop";
		if(ext::filesystem::exists(p))
			return p;

		p = boost::filesystem::path(_getenv("HOME"));
		if(ext::filesystem::exists(p)) 
			return p;

		/* Should never happen */
		char dirname_template[] = "uftt-XXXXXX";
		char* dirname = mkdtemp(dirname_template);
		p = boost::filesystem::path(dirname);
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
		char* name = NULL;
		if (!name) name = getenv("USER");
		if (!name) name = getenv("USERNAME");
		if (!name) name = "uftt-user";
		return name;
	}

} // namespace platform
