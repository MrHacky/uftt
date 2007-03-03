#ifndef SHARELISTER_H
#define SHARELISTER_H

#include "stdafx.h"
#include <dirent.h>

struct FileInfo {
	std::string name;
	uint32 attrs;
	uint64 size;
	std::vector<FileInfo*> file;
	uint64 UID;
	FileInfo(std::string path);
	// TODO: ~FileInfo
};

struct ShareInfo {
	std::string name;
	FileInfo* root;
	uint64 UID;
	ShareInfo(std::string path);
	// TODO: ~ShareInfo
};

struct ServerInfo {
	std::string name;
	sockaddr* address;
	std::vector<ShareInfo*> share;

	void add_share(ShareInfo* share);
};

void init_server_list();

#endif //SHARELISTER_H
