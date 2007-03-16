#include "stdafx.h"
#include "sharelister.h"
#include "packet.h"
#include "yarn.h"
#include "main.h"

//TODO: Put into stdafx.h
#include <fstream>

using namespace std;

int gatherServers(void *){
	fprintf(stderr, "Gathering available servers...\n");
	UFTT_packet pack;
	pack.type = PT_QUERY_SERVERS;
	send_packet(&pack, ServerSock);
	ThreadExit(0);
}

int get_sharelist(ServerInfo * si) {
	fprintf(stderr, "Gathering sharelist from server %s...\n", addr2str(si->address).c_str());
	UFTT_packet_shareinfo p;
	p.type = PT_QUERY_SHARELIST;
	p.serverinfo = si;
	send_packet((UFTT_packet *)&p, ServerSock);
	ThreadExit( 0 );
}


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
	f.close();
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

vector<ServerInfo*> servers;
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

FileInfo::FileInfo(const std::string& path) {
	//fprintf(stderr,"FileInfo: Traversing %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());

	attrs = 0;

	string::const_iterator iter;
	for (iter = path.end(); iter != path.begin() && *(--iter) != '/'; ); // FIXME: '\\' instead of '/' on windows?
	if (iter != path.end()) ++iter;
	name = string(iter, path.end());

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
		closedir(dir);
		sort(file.begin(), file.end(), filecomp());
	}
	else { // is file
		if (errno != 20) // FIXME: Use named constant
			fprintf(stderr,"Error: %s\n Error=%i:%s\n",path.c_str(), errno , strerror(errno));
		size = FileSize(path.c_str());
	}
}

ShareInfo::ShareInfo(const std::string& path) {
	fprintf(stderr,"Parsing %s\n",path.c_str());
	root = new FileInfo(path);
	name = path;
}

void ServerInfo::add_share(ShareInfo* ashare) {
	share.push_back(ashare); // TODO: more?
	fprintf(stderr,"TODO: Adding share to server\n");
}



