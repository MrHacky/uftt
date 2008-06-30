#include "Platform.h"

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#else
#include <unistd.h>
#include <stdlib.h>
#endif

#include <boost/filesystem.hpp>

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
		long lStdHandle;
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		FILE *fp;
		// allocate a console for this app
		AllocConsole();
		// set the screen buffer to be big enough to let us scroll text
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
		coninfo.dwSize.Y = 500;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
		// redirect unbuffered STDOUT to the console
		lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
		*stdout = *fp;
		setvbuf( stdout, NULL, _IONBF, 0 );
		// redirect unbuffered STDIN to the console
		lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "r" );
		*stdin = *fp;
		setvbuf( stdin, NULL, _IONBF, 0 );
		// redirect unbuffered STDERR to the console
		lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
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

		command = cmd;

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
				(flags & RF_NEW_CONSOLE) ? CREATE_NEW_CONSOLE : 0,
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

	spathlist getSettingPathList() {
		spathlist result;
		boost::filesystem::path currentdir(boost::filesystem::current_path<boost::filesystem::path>());
		result.push_back(spathinfo("Current Directory"      , currentdir / "uftt.dat"));
#ifdef WIN32
		result.push_back(spathinfo("User Documents"         , getFolderLocation(CSIDL_MYDOCUMENTS)    / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat"));
		result.push_back(spathinfo("Common Application Data", getFolderLocation(CSIDL_COMMON_APPDATA) / "UFTT" / "uftt.dat"));
#else
		result.push_back(spathinfo("Home Directory"         , boost::filesystem::path(getenv("HOME")) / ".uftt" / "uftt.dat"));
#endif
		return result;
	}

	void msSleep(int ms)
	{
#ifdef WIN32
		Sleep(ms);
#else
		usleep(ms*1000);
#endif
	}

} // namespace platform
