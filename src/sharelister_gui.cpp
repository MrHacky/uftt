#include "sharelister_gui.h"

using namespace std;

class FileEntry {
	public:
	GtkTreeIter iter;
	FileInfo*   file;
};

GtkTreeModel * WINAPI
add_tree_data(  GtkTreeView* aview, ShareInfo* share)
{
	GtkTreeStore  *store;
	GtkTreeIter    iter;
	GtkTreeIter    iter2;

	vector<FileEntry> flist;
	FileEntry fentry;

	store = GTK_TREE_STORE (gtk_tree_view_get_model( aview ));

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter,
											COL_NAME, share->name.c_str(),
											COL_SIZE, (int32)share->root->size,
											-1);

	fentry.iter = iter;
	fentry.file = share->root;

	flist.push_back(fentry);

	while (flist.size() > 0) {
		fentry = flist.back();
		flist.pop_back();

		gtk_tree_store_append (store, &iter, &fentry.iter);
		gtk_tree_store_set (store, &iter,
												COL_NAME, fentry.file->name.c_str(),
												COL_SIZE, (int32)fentry.file->size,
												-1);

		for (int i = 0; i < fentry.file->file.size(); ++i) {
			FileEntry newentry;
			newentry.file = fentry.file->file[i];
			newentry.iter = iter;
			flist.push_back(newentry);
		}
	}

/*
	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter,
											COL_NAME, "Heinz El-Mann",
											COL_SIZE, 51,
											-1);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter,
											COL_NAME, "Heinz El-Mann",
											COL_AGE, 51,
											-1);

	iter2 = iter;
	gtk_tree_store_append (store, &iter, &iter2);
	gtk_tree_store_set (store, &iter,
											COL_NAME, "Jane Doe",
											COL_AGE, 23,
											-1);

	iter2 = iter;
	gtk_tree_store_append (store, &iter, &iter2);
	gtk_tree_store_set (store, &iter,
											COL_NAME, "Joe Bungop",
											COL_AGE, 91,
											-1);
*/
	return GTK_TREE_MODEL (store);
}

GtkWidget * WINAPI
init_tree_view (GtkTreeView* aview)
{
	GtkCellRenderer     *renderer;
	GtkTreeModel        *model;
	GtkWidget           *view = (GtkWidget*)aview;

	/* --- Column #1 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
																							 -1,
																							 "Name",
																							 renderer,
																							 "text", COL_NAME,
																							 NULL);

	/* --- Column #2 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
																							 -1,
																							 "Size",
																							 renderer,
																							 "text", COL_SIZE,
																							 NULL);

	model = GTK_TREE_MODEL (gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_UINT));

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

	/* The tree view has acquired its own reference to the
	*  model, so we can drop ours. That way the model will
	*  be freed automatically when the tree view is destroyed */

	g_object_unref (model);

	return view;
}
