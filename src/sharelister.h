#ifndef SHARELISTER_H
#define SHARELISTER_H

#include "stdafx.h"
enum {
	COL_NAME = 0,
	COL_AGE,
	NUM_COLS
} ;

struct FileInfo {
	std::string name;
	uint32 attrs;
	std::vector<FileInfo*> file;
	uint64 UID;
};

struct ShareInfo {
	std::string name;
	FileInfo* share;
	uint64 UID;
};

struct ServerInfo {
	std::string name;
	sockaddr* address;
	std::vector<ShareInfo*> share;
	uint64 UID;
};

ShareInfo* create_share_from_uri(guchar * uri);

GtkTreeModel * WINAPI
create_and_fill_model( void );

GtkWidget * WINAPI
create_view_and_model( GtkTreeView* aview );

void add_server_to_list(struct ServerInfo);
void remove_server_from_list(uint64 UID);
void add_share_to_server(uint64 UID, struct ShareInfo share);
void remove_share_from_server(uint64 serverUID, uint64 shareUID);
void add_file_to_share(uint64 shareUID, struct FileInfo file);
void remove_file_from_share(uint64 shareUID, uint64 fileUID);
#endif //SHARELISTER_H
