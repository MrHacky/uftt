#ifndef SHARELISTER_H
#define SHARELISTER_H

#include "stdafx.h"

enum FileAttrs {
	FATTR_DIR = 1 << 0,
};

struct FileInfo {
	std::string name;
	uint32 attrs;
	uint64 size;
	std::vector<FileInfo*> file;
	uint64 UID;
	FileInfo(const std::string& path);
	// TODO: ~FileInfo
};

struct ShareInfo {
	std::string name;
	FileInfo* root;
	uint64 UID;
	ShareInfo(const std::string& path);
	// TODO: ~ShareInfo
};

struct ServerInfo {
	std::string name;
	sockaddr* address;
	std::vector<ShareInfo*> share;

	void add_share(ShareInfo* ashare);
};

void init_server_list();
int gatherServers(void *);
int get_sharelist(ServerInfo * si);

extern std::vector<ServerInfo*> servers;
extern ServerInfo *myServer;

#endif //SHARELISTER_H
