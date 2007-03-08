#include "stdafx.h"
#include "sharelister.h"

//TODO: Put into stdafx.h
#include <fstream>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>


using namespace std;

/* 
WIN32_FIND_DATA f;
HANDLE h = FindFirstFile("./*", &f);
if(h != INVALID_HANDLE_VALUE)
{
do
{
puts(f.cFileName);
} while(FindNextFile(h, &f));
*/

// TODO: move this somewhere else?
uint64 FileSize(const char* sFileName) {
#ifndef G_OS_WIN32
	std::ifstream f;
	f.open(sFileName, std::ios_base::binary | std::ios_base::in);
	if (!f.good() || f.eof() || !f.is_open()) { return 0; }
	f.seekg(0, std::ios_base::beg);
	std::ifstream::pos_type begin_pos = f.tellg();
	f.seekg(0, std::ios_base::end);
	uint64 ftg=0, bp=0;
	ftg=f.tellg();
	bp=begin_pos;
	return static_cast<uint64>(ftg-bp);
#else //def G_OS_WIN32
	int f;
	if((_sopen_s( &f, sFileName, _O_RDWR, _SH_DENYNO, _S_IREAD ))==0) {
		uint64 s= _filelengthi64( f );
		_close(f);
		return s;
	}
	else {
		return 0;
	}
#endif
}

vector<ServerInfo> servers;
ServerInfo *myServer;  //ptr into server list (our own server also resides in the share list :)

void init_server_list() {
	ServerInfo *server = new ServerInfo;
	myServer = server;
	myServer->name = string("localhost");
	myServer->address = NULL;
}

struct filecomp : public binary_function<FileInfo*, FileInfo*, bool> {
	bool operator()(FileInfo* x, FileInfo* y) {
		if ((x->attrs & FATTR_DIR) != (y->attrs & FATTR_DIR))
			return (x->attrs & FATTR_DIR);
		return x->name < y->name;
	}
};

FileInfo::FileInfo(std::string path) {
	fprintf(stderr,"FileInfo: Traversing %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());

	attrs = 0;
	
	int i = 0;
	for (i = path.size(); i > 0 && path[--i] != '/'; ); // FIXME: '\\' instead of '/' on windows?
	name = string(&(path.c_str())[++i]); // FIXME: evul hack;
	
	if(dir!=NULL){ // is dir
		attrs |= FATTR_DIR;
		size = 0;
		dirent *fent; // file entry
		while((fent=readdir (dir))!=NULL) {
			if(string(fent->d_name) != "." && string(fent->d_name) != "..") {
				FileInfo* newfile =new FileInfo(path+"/"+fent->d_name);
				file.push_back(newfile);
				size += newfile->size;
			}
		}
		sort(file.begin(), file.end(), filecomp());
	}
	else { // is file
		size = FileSize(path.c_str());
		//fprintf(stderr,"FileInfo: File: %s\n (errval=%i)",path.c_str(), errno);
	}
}

ShareInfo::ShareInfo(std::string path) {
	fprintf(stderr,"Parsing %s\n",path.c_str());
	root = new FileInfo(path.c_str());
	name = path;
}

void ServerInfo::add_share(ShareInfo* share) {
	//TODO: void add_share_to_server(uint64 UID, const struct ShareInfo &share);
	fprintf(stderr,"TODO: Adding share to server\n");
}



