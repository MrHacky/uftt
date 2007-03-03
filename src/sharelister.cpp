#include "stdafx.h"
#include "sharelister.h"
#include <dirent.h>

using namespace std;

vector<ServerInfo> servers;
ServerInfo *myServer;  //ptr into server list (our own server also resides in the share list :)

void init_server_list(){
	ServerInfo *server = new ServerInfo;
	myServer = server;
	myServer->name = string("localhost");
	myServer->address = NULL;
}

FileInfo::FileInfo(std::string path) {
	fprintf(stderr,"FileInfo: Traversing %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	if(dir!=NULL){ // is dir
		size = 0;
		dirent *fent; // file entry
		while((fent=readdir (dir))!=NULL) {
			if(string(fent->d_name) != "." && string(fent->d_name) != "..") {
				FileInfo* newfile =new FileInfo(path+"/"+fent->d_name);
				newfile->name = string(fent->d_name); // not really the right place for this...
				file.push_back(newfile);
				size += newfile->size;
			}
		}
	}
	else { // is file
		size = 10; /* TODO: get file size */
		name = 
		fprintf(stderr,"FileInfo: File: %s\n (errval=%i)",path.c_str(), errno);
	}
}

ShareInfo::ShareInfo(std::string path) {
	fprintf(stderr,"Parsing %s\n",path.c_str());
	root = new FileInfo(path.c_str());
	int i = 0;
	for (int j = 0; j < path.size(); ++j)
		if (path[j]=='/') i = j + 1;
	root->name = string(&(path.c_str())[i]); // FIXME: evul hack;
	name = path;
}

void ServerInfo::add_share(ShareInfo* share)
{
	//TODO: void add_share_to_server(uint64 UID, const struct ShareInfo &share);
	fprintf(stderr,"TODO: Adding share to server\n");
}



