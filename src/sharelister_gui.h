#ifndef SHARELISTER_GUI_H
#define SHARELISTER_GUI_H

#include "stdafx.h"
#include "sharelister.h"

enum {
//	COL_PIX  = 0,
	COL_NAME =0,
	COL_SIZE,
	NUM_COLS
} ;

GtkTreeModel * WINAPI
add_tree_data(  GtkTreeView* aview, ShareInfo* share);

GtkWidget * WINAPI
init_tree_view( GtkTreeView* aview );

#endif //SHARELISTER_GUI_H
