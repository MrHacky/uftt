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
	FileInfo* root;
	uint64 UID;
	ShareInfo(std::string uri);
};

struct ServerInfo {
	std::string name;
	sockaddr* address;
	std::vector<ShareInfo*> share;

	void add_share(ShareInfo* share);
};

ShareInfo* create_share_from_uri(guchar * uri);

GtkTreeModel * WINAPI
create_and_fill_model( void );

GtkWidget * WINAPI
create_view_and_model( GtkTreeView* aview );

void init_server_list();

#endif //SHARELISTER_H
