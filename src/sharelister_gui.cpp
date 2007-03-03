#include "sharelister_gui.h"

using namespace std;

class FileEntry {
	public:
	GtkTreeIter iter;
	FileInfo*   file;
};

string strsize(const int64 size)
{
	const char* size_suffix[] = {"", "K","M","G","T","P","?"};
	double fsize = size;
	int pfix = 0;
	for (;fsize > 999; ++pfix) fsize /= 1024;
	if (pfix > 6) pfix = 6;
	int decs = 0;
	if (pfix > 0 && fsize < 100) ++decs;
	if (pfix > 0 && fsize < 10 ) ++decs;
	char buf[127];
	snprintf(buf, 127, "%.*lf%sB", decs, fsize, size_suffix[pfix]);
	return buf;
} 

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
											COL_SIZE, strsize(share->root->size).c_str(),
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
												COL_SIZE, strsize(fentry.file->size).c_str(),
												-1);

		/* our list is used as a stack, so add stuff in reverse to preserver ordering */
		for ( vector<FileInfo*>::reverse_iterator rfiter = fentry.file->file.rbegin();
						rfiter != fentry.file->file.rend();
						++rfiter)
		{
			FileEntry newentry;
			newentry.file = *rfiter;
			newentry.iter = iter;
			flist.push_back(newentry);
		}
	}

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

	model = GTK_TREE_MODEL (gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING));

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

	/* The tree view has acquired its own reference to the
	*  model, so we can drop ours. That way the model will
	*  be freed automatically when the tree view is destroyed */

	g_object_unref (model);

	return view;
}
