#include "sharelister_gui.h"

#include "folder_open.xpm"
#include "folder_closed.xpm"
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
	snprintf(buf, 127, "%.*lf %sB", decs, fsize, size_suffix[pfix]);
	return buf;
}

GtkTreeModel * WINAPI
add_tree_data(  GtkTreeView* aview, ShareInfo* share) {
	GtkTreeStore  *store;
	GtkTreeIter    iter;

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

		/* our list is used as a stack, so add stuff in reverse to preserve ordering */
		for ( vector<FileInfo*>::reverse_iterator rfiter = fentry.file->file.rbegin();
						rfiter != fentry.file->file.rend();
						++rfiter)
		{
			FileEntry newentry;
			newentry.file = *rfiter;
			newentry.iter = iter;
			flist.push_back(newentry);
		}
		/*if (fentry.file->attrs & FATTR_DIR){
			GtkTreeIter    newiter;
			gtk_tree_store_append (store, &newiter, &iter);
			gtk_tree_store_set (store, &newiter,
													COL_NAME, "",
													COL_SIZE, "",
													-1);
		}*/
	}

	return GTK_TREE_MODEL (store);
}

GtkWidget * WINAPI
init_tree_view (GtkTreeView* aview) {
	GtkTreeModel        *model;
	GtkWidget           *view = (GtkWidget*)aview;
	GtkCellRenderer     *renderer = gtk_cell_renderer_pixbuf_new ();;
	GdkPixbuf* p1 = gdk_pixbuf_new_from_xpm_data((const char**)folder_open_xpm);
	GdkPixbuf* p2 = gdk_pixbuf_new_from_xpm_data((const char**)folder_closed_xpm);
	g_object_set(renderer,"pixbuf-expander-open",p1,NULL);
	g_object_set(renderer,"pixbuf-expander-closed",p2,NULL);

	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Name", renderer);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
	                                     "pixbuf-expander-closed", p2,
	                                     "pixbuf-expander-open", p1,
	                                     NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COL_NAME,
					     NULL);
	gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, -1);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
	                                             -1,
	                                             "Size",
	                                             renderer,
	                                             "text", COL_SIZE,
	                                             NULL);

	model = GTK_TREE_MODEL (gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

	GValue *\
	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_BOOLEAN);
	g_value_set_boolean (gval, true);
	g_object_set_property (G_OBJECT (aview), "enable-tree-lines", gval);
	g_free (gval);

	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_BOOLEAN);
	g_value_set_boolean (gval, true);
	g_object_set_property (G_OBJECT (aview), "show-expanders", gval);
	g_free (gval);

	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_INT);
	g_value_set_int (gval, 0);
	g_object_set_property (G_OBJECT (aview), "level-indentation", gval);
	g_free (gval);

	g_object_unref (model);
	return view;


/******************************************************************************************************************/


	renderer = gtk_cell_renderer_pixbuf_new ();

/*	FIXME: Does not verk in Windows * /
	GdkPixbuf* p;
	GError *err = 0;
	p = gdk_pixbuf_new_from_file("../../src/folder_open.gif", &err);
	g_object_set(renderer,"pixbuf-expander-open",p,NULL);
	p = gdk_pixbuf_new_from_file("../../src/folder_closed.gif", &err);
	g_object_set(renderer,"pixbuf-expander-closed",p,NULL);
			//p = gdk_pixbuf_new_from_file("../../src/folder_closed.gif", &err);
			//g_object_set(renderer,"pixbuf",p,NULL);
/*/
/*	GdkPixbuf* p1,*p2;
	GError *err = 0;
	p1 = gdk_pixbuf_new_from_xpm_data((const char**)folder_open_xpm);
	fprintf(stderr, "ERROR = %i / \n",err);
	//g_object_set(renderer,"pixbuf-expander-open",p,NULL);
	p2 = gdk_pixbuf_new_from_xpm_data((const char**)folder_closed_xpm);
	fprintf(stderr, "ERROR = %i / \n",err);
	//g_object_set(renderer,"pixbuf-expander-closed",p,NULL);

/*	GdkPixmap*  gdk_pixmap_create_from_xpm      (GdkWindow *window,
                                             GdkBitmap **mask,
                                             GdkColor *transparent_color,
                                             const gchar *filename);
/*********************************************************************/


/*	GtkTreeViewColumn*  column ;/*= gtk_tree_view_column_new_with_attributes(
			"Name",
			renderer,
			NULL);
			*/

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

//	g_object_set_property (G_OBJECT (aview), "expander-column", (GValue*)collumn);


	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf-expander-closed", p2,
					     "pixbuf-expander-open", p1,
					     NULL);


	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COL_NAME,
					     NULL);

	gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, -1);

	//g_object_set (G_OBJECT (aview),"expander-column", (GValue*)collumn, "headers-visible", TRUE, NULL);


	/* --- Column #1 --- */
	/*renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
																							 -1,
																							 "Name",
																							 renderer,
																							 "text", COL_NAME,
																							 NULL);
*/
	/* --- Column #2 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
																							 -1,
																							 "Size",
																							 renderer,
																							 "text", COL_SIZE,
																							 NULL);

	model = GTK_TREE_MODEL (gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

//	GValue *gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_BOOLEAN);
	g_value_set_boolean (gval, true);
	g_object_set_property (G_OBJECT (aview), "enable-tree-lines", gval);
	g_free (gval);

	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_BOOLEAN);
	g_value_set_boolean (gval, false);
	//g_object_set_property (G_OBJECT (aview), "show-expanders", gval);
	g_free (gval);

	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_INT);
	g_value_set_int (gval, 10);
	//g_object_set_property (G_OBJECT (aview), "level-indentation", gval);
	g_free (gval);

	/* The tree view has acquired its own reference to the
	*  model, so we can drop ours. That way the model will
	*  be freed automatically when the tree view is destroyed */

	g_object_unref (model);

	return view;
}
