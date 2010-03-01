#include "ShareList.h"
#include "../util/StrFormat.h"
#include "../util/Filesystem.h"
#include <string>
#include <gdkmm/event.h>
#include <gtkmm/stock.h>
#include <boost/foreach.hpp>
#include <gtkmm/messagedialog.h>
#include <gtkmm/treeviewcolumn.h>

using namespace std;

ShareList::ShareList(UFTTSettingsRef _settings, Gtk::Window& _parent_window)
: settings(_settings),
  parent_window(_parent_window),
  browse_for_download_destination_path_button("Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER)
{
	share_list_liststore = Gtk::ListStore::create(share_list_columns);
//	share_list_treeview.set_model(SortableTreeDragDest<Gtk::ListStore>::create(share_list_liststore)); // FIXME: Enabling this causes Gtk to give a silly warning
	share_list_treeview.set_model(share_list_liststore);
	#define ADD_TV_COL_SORTABLE(tv, title, column) tv.get_column(tv.append_column(title, column) - 1)->set_sort_column(column);
	ADD_TV_COL_SORTABLE(share_list_treeview, "User Name" , share_list_columns.user_name);
	ADD_TV_COL_SORTABLE(share_list_treeview, "Share Name", share_list_columns.share_name);
	ADD_TV_COL_SORTABLE(share_list_treeview, "Host Name" , share_list_columns.host_name);
	ADD_TV_COL_SORTABLE(share_list_treeview, "Protocol"  , share_list_columns.protocol);
	ADD_TV_COL_SORTABLE(share_list_treeview, "URL"       , share_list_columns.url);
	#undef ADD_TV_COL_SORTABLE
	share_list_treeview.set_headers_clickable(true);
	share_list_treeview.set_rules_hint(true);
	share_list_treeview.set_search_column(share_list_columns.share_name);
	share_list_liststore->set_sort_column(share_list_columns.host_name, Gtk::SORT_ASCENDING);
	BOOST_FOREACH(Gtk::TreeViewColumn* column, share_list_treeview.get_columns()) {
		column->set_reorderable(true);
		column->set_resizable(true);
	}
	share_list_treeview.set_enable_search(true);
	share_list_treeview.set_rubber_banding(true);
	share_list_treeview.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	std::list<Gtk::TargetEntry> listTargets;
	listTargets.push_back( Gtk::TargetEntry("text/uri-list", Gtk::TARGET_OTHER_APP, 0) ); // Last parameter is 'Info', used to distinguish
	listTargets.push_back( Gtk::TargetEntry("text/plain"   , Gtk::TARGET_OTHER_APP, 1) ); // different types of TargetEntry in the drop handler
	listTargets.push_back( Gtk::TargetEntry("STRING"       , Gtk::TARGET_OTHER_APP, 2) );
	share_list_treeview.drag_dest_set(listTargets); // Should use defaults, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
	share_list_treeview.signal_drag_data_received().connect(boost::bind(&ShareList::on_share_list_treeview_signal_drag_data_received, this, _1, _2, _3, _4, _5, _6));
	// FIXME: TreeView deselects rows before calling activated when pressing enter.
	//         See http://bugzilla.xfce.org/show_bug.cgi?id=5943
	share_list_treeview.signal_row_activated().connect(boost::bind(&ShareList::download_selected_shares, this));
	share_list_treeview.signal_button_press_event().connect(
		sigc::mem_fun(this, &ShareList::on_share_list_treeview_signal_button_press_event), false);
	share_list_scrolledwindow.add(share_list_treeview);
	share_list_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	on_download_destination_path_entry_signal_changed_connection =
		download_destination_path_entry.signal_changed().connect(
			boost::bind(&ShareList::on_download_destination_path_entry_signal_changed, this));
	download_destination_path_entry.set_text(settings->dl_path.native_directory_string());
	download_destination_path_hbox.add(download_destination_path_entry);
	download_destination_path_hbox.pack_start(browse_for_download_destination_path_button, Gtk::PACK_SHRINK);
	browse_for_download_destination_path_button.signal_current_folder_changed().connect( // FIXME: Dialog is not modal
		boost::bind(&ShareList::on_browse_for_download_destination_path_button_signal_current_folder_changed, this));
	download_destination_path_label.set_alignment(0.0f, 0.5f);
	download_destination_path_label.set_text("Download destination folder:");
	download_destination_path_vbox.add(download_destination_path_label);
	download_destination_path_vbox.add(download_destination_path_hbox);
	download_destination_path_alignment.add(download_destination_path_vbox);
	download_destination_path_alignment.set_padding(8, 4, 4, 4);

	download_destination_path_entry.set_tooltip_text("Enter the folder where downloaded shares should be placed");
	browse_for_download_destination_path_button.set_tooltip_text("Select the folder where downloaded shares should be placed");

	this->add(share_list_scrolledwindow);
	this->pack_start(download_destination_path_alignment, Gtk::PACK_SHRINK);
}

void ShareList::set_popup_menu(Gtk::Menu* _on_row_popup_menu, Gtk::Menu* _not_on_row_popup_menu) {
	on_row_popup_menu     = _on_row_popup_menu;
	not_on_row_popup_menu = _not_on_row_popup_menu;
}

bool ShareList::on_share_list_treeview_signal_button_press_event(GdkEventButton* event) {
	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		Gtk::TreeModel::Path  path;
		Gtk::TreeViewColumn* column;
		int    cell_x, cell_y;
		bool exists = share_list_treeview.get_path_at_pos((int)(event->x), (int)(event->y), path, column, cell_x, cell_y);
		if(exists) {
			if(!share_list_treeview.get_selection()->is_selected(path)) {
				share_list_treeview.get_selection()->unselect_all();
				share_list_treeview.get_selection()->select(path);
			}
			if(on_row_popup_menu != NULL)
				on_row_popup_menu->popup(event->button, event->time);
			return true;
		}
		else {
			if(not_on_row_popup_menu != NULL)
				not_on_row_popup_menu->popup(event->button, event->time);
			return true;
		}
	}
	return false;
}

void ShareList::set_backend(UFTTCore* _core) {
	core = _core;
	core->connectSigAddShare(dispatcher.wrap(boost::bind(&ShareList::on_signal_add_share, this, _1)));
}

void ShareList::on_download_destination_path_entry_signal_changed() {
	settings->dl_path = download_destination_path_entry.get_text();
	while(settings->dl_path.leaf() == ".") settings->dl_path.remove_leaf(); // Remove trailing '/' 's
	if(!ext::filesystem::exists(settings->dl_path)) {
		download_destination_path_entry.modify_text(Gtk::STATE_NORMAL, Gdk::Color("#000000"));
		// Note: some windows app uses #ffb3b3 and black text, anjuta uses #ff6666 and white text (the average is about #ff9494)
		download_destination_path_entry.modify_base(Gtk::STATE_NORMAL, Gdk::Color("#ffb3b3"));
	}
	else {
		download_destination_path_entry.unset_base(Gtk::STATE_NORMAL);
		download_destination_path_entry.unset_text(Gtk::STATE_NORMAL);
		on_download_destination_path_entry_signal_changed_connection.block();
		browse_for_download_destination_path_button.set_current_folder(settings->dl_path.string());
	}
}

void ShareList::on_browse_for_download_destination_path_button_signal_current_folder_changed() {
	boost::filesystem::path pa(download_destination_path_entry.get_text());
	boost::filesystem::path pb(browse_for_download_destination_path_button.get_current_folder());
	while(pa.leaf() == ".") pa.remove_leaf();
	while(pb.leaf() == ".") pb.remove_leaf();
	
	if((!on_download_destination_path_entry_signal_changed_connection.blocked()) && (pa.string() != pb.string())) {
		// If this connection is blocked it means we are being called because the
		// entry is updating the current_folder of the filechooserbutton. Don't
		// try to tell the entry it is wrong, it is right!
		// The compare on pa and pb is needed because the filechooser seems to
		// always strip trailing path separators, and we don't want to undo a '/'
		// typed by the user.
		download_destination_path_entry.set_text(browse_for_download_destination_path_button.get_current_folder());
	}
	on_download_destination_path_entry_signal_changed_connection.unblock();
}

string urldecode(std::string s) { //http://www.koders.com/cpp/fid6315325A03C89DEB1E28732308D70D1312AB17DD.aspx
	std::string buffer = "";
	int len = s.length();

	for (int i = 0; i < len; i++) {
		int j = i ;
		char ch = s.at(j);
		if (ch == '%'){
			char tmpstr[] = "0x0__";
			int chnum;
			tmpstr[3] = s.at(j+1);
			tmpstr[4] = s.at(j+2);
			chnum = strtol(tmpstr, NULL, 16);
			buffer += chnum;
			i += 2;
		} else {
			buffer += ch;
		}
	}
	return buffer;
}

vector<std::string> urilist_convert(const std::string urilist) {
	vector<std::string> files;
	int begin = urilist.find("file://", 0);
	int end = urilist.find("\r\n", begin);
	while( end != string::npos) {
		files.push_back(urldecode(urilist.substr(begin + 7, end-begin-7)));
		begin = urilist.find("file://", end);
		if(begin == string::npos) break;
		end = urilist.find("\r\n", begin);
	}
	return files;
}

void ShareList::on_share_list_treeview_signal_drag_data_received(
	const Glib::RefPtr<Gdk::DragContext>& context, 
	int x, int y,
	const Gtk::SelectionData& selection_data, guint info, guint time)
{
	// Note: We know we only receive strings (of type STRING, text/uri-list and text/plain)
	//       so we could use something similar to g_uri_list_extract_uris, but this is not
	//       present in GlibMM (yet?). The single line version filename_from_uri is, but
	//       we have an uri-list...
	switch(info) {
		case 0: {
			BOOST_FOREACH(std::string file, urilist_convert(selection_data.get_data_as_string())) {
				#ifdef WIN32
					file = file.substr(1);
				#endif
				boost::filesystem::path path = file;
				if (path.leaf() == ".") // linux thingy
					path.remove_leaf();
				core->addLocalShare(path.leaf(), path);
			}
		}; break;
		case 2:
		case 1: {
				boost::filesystem::path path = selection_data.get_data_as_string();
				if (path.leaf() == ".") // linux thingy
					path.remove_leaf();
				core->addLocalShare(path.leaf(), path);
		}; break;
		default:
			cout << "Warning: unhandled drop event!" << endl;
			context->drag_finish(false, false, time);
			return;
	}
	context->drag_finish(true, false, time);
}


void ShareList::download_selected_shares() {
	//FIXME: Don't forget to test dl_path for validity and writeablity
	if(!ext::filesystem::exists(settings->dl_path)) {
		Gtk::MessageDialog dialog("Download destination folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text("The folder you have selected for downloaded shares to be placed in does not appear to exist.\nPlease select another download destination and try again.");
		dialog.set_transient_for(parent_window);
		dialog.set_modal(true);
		dialog.run();
		return;
	}
	Glib::RefPtr<Gtk::TreeSelection> sel = share_list_treeview.get_selection();
	BOOST_FOREACH(Gtk::TreeModel::Path p, sel->get_selected_rows()) {
		const Gtk::TreeModel::Row& row = *(share_list_liststore->get_iter(p));
		ShareID sid = row[share_list_columns.share_id];
		{ // evil hax!!!
			Gdk::ModifierType mask;
			int x,y;
			get_window()->get_pointer(x, y, mask);
			if((mask & Gdk::SHIFT_MASK) == Gdk::SHIFT_MASK) {
				sid.sid[0] = 'x';
			}
		}
		core->startDownload(sid, settings->dl_path);
	}
}

void ShareList::clear() {
	share_list_liststore->clear();
}

void ShareList::on_signal_add_share(const ShareInfo& info) {
	if (info.isupdate) { // FIXME: Should be handled by core
		return;
	}

	uint32 version = atoi(info.proto.substr(6).c_str()); // FIXME: Perhaps put this type of check in the core
	bool found = false;
	BOOST_FOREACH(const Gtk::TreeRow& row, share_list_liststore->children()) {
		if((row[share_list_columns.host_name] == info.host) && (row[share_list_columns.share_name] == info.name)) {
			found = true;
			uint32 over = atoi(((Glib::ustring)row[share_list_columns.protocol]).substr(6).c_str());
			if(over <= version) {
				row[share_list_columns.user_name] = info.user;
				row[share_list_columns.protocol]  = info.proto;
				row[share_list_columns.url]       = STRFORMAT("%s://%s/%s", info.proto, info.host, info.name);
			}
		}
	}
	if(!found) {
		Gtk::TreeModel::iterator i = share_list_liststore->append();
		(*i)[share_list_columns.user_name]  = info.user;
		(*i)[share_list_columns.share_name] = info.name;
		if(info.name == "") (*i)[share_list_columns.share_name] = "Anonymous";
		(*i)[share_list_columns.host_name]  = info.host;
		(*i)[share_list_columns.protocol]   = info.proto;
		(*i)[share_list_columns.url]        = STRFORMAT("%s://%s/%s", info.proto, info.host, info.name);
		(*i)[share_list_columns.share_id]   = info.id;
	}
}
