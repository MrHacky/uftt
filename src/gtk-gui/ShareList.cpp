#include "ShareList.h"
#include "../util/StrFormat.h"
#include <string>
#include <gdkmm/event.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm/stock.h>
#include <boost/foreach.hpp>
#include <gtkmm/messagedialog.h>
#include <gtkmm/treeviewcolumn.h>
#include <boost/lambda/if.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

using namespace std;

ShareList::ShareList(UFTTSettingsRef _settings, Gtk::Window& _parent_window, Glib::RefPtr<Gtk::UIManager> uimanager_ref_)
: settings(_settings),
  pick_download_destination_dialog(_parent_window, "Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  add_share_file_dialog(_parent_window, "Select a file", Gtk::FILE_CHOOSER_ACTION_OPEN),
  add_share_folder_dialog(_parent_window, "Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  browse_for_download_destination_path_button("Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  uimanager_ref(uimanager_ref_)
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
	share_list_treeview.get_selection()->signal_changed().connect(
		boost::bind(&ShareList::on_share_list_treeview_selection_signal_changed, this)
	);
	std::vector<Gtk::TargetEntry> listTargets;
	listTargets.push_back( Gtk::TargetEntry("text/uri-list", Gtk::TARGET_OTHER_APP, 0) ); // Last parameter is 'Info', used to distinguish
	listTargets.push_back( Gtk::TargetEntry("text/plain"   , Gtk::TARGET_OTHER_APP, 1) ); // different types of TargetEntry in the drop handler
	listTargets.push_back( Gtk::TargetEntry("STRING"       , Gtk::TARGET_OTHER_APP, 2) );
	share_list_treeview.drag_dest_set(listTargets); // Should use defaults, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
	share_list_treeview.signal_drag_data_received().connect(boost::bind(&ShareList::on_share_list_treeview_signal_drag_data_received, this, _1, _2, _3, _4, _5, _6));
	share_list_treeview.signal_row_activated().connect(boost::bind(&ShareList::download_selected_shares, this));
	share_list_treeview.signal_button_press_event().connect(
		sigc::mem_fun(this, &ShareList::on_share_list_treeview_signal_button_press_event), false);
	share_list_treeview.signal_key_press_event().connect(
		sigc::mem_fun(this, &ShareList::on_share_list_treeview_signal_key_press_event), false);

	share_list_scrolledwindow.add(share_list_treeview);
	share_list_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	on_download_destination_path_entry_signal_changed_connection =
		download_destination_path_entry.signal_changed().connect(
			boost::bind(&ShareList::on_download_destination_path_entry_signal_changed, this));
	download_destination_path_entry.set_text(settings->dl_path.get().native_directory_string());
	download_destination_path_hbox.add(download_destination_path_entry);
	download_destination_path_hbox.pack_start(browse_for_download_destination_path_button, Gtk::PACK_SHRINK);
	browse_for_download_destination_path_button.signal_current_folder_changed().connect( // FIXME: Dialog is not modal
		boost::bind(&ShareList::on_browse_for_download_destination_path_button_signal_current_folder_changed, this));
	download_destination_path_label.set_alignment(0.0f, 0.5f);
	download_destination_path_label.set_use_underline(true);
	download_destination_path_label.set_text_with_mnemonic("_Download destination folder:");
	download_destination_path_label.set_mnemonic_widget(download_destination_path_entry);
	download_destination_path_vbox.add(download_destination_path_label);
	download_destination_path_vbox.add(download_destination_path_hbox);
	download_destination_path_alignment.add(download_destination_path_vbox);
	download_destination_path_alignment.set_padding(8, 4, 4, 4);

	download_destination_path_entry.set_tooltip_text("Enter the folder where downloaded shares should be placed");
	browse_for_download_destination_path_button.set_tooltip_text("Select the folder where downloaded shares should be placed");

	this->add(share_list_scrolledwindow);
	this->pack_start(download_destination_path_alignment, Gtk::PACK_SHRINK);

	{
		Gtk::Button* button;
		button = add_share_file_dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		button->signal_clicked().connect(boost::bind(&Gtk::Widget::hide, &add_share_file_dialog));
		button = add_share_file_dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
		button->set_label("_Select");
		Gtk::Image* image = Gtk::manage(new Gtk::Image()); // FIXME: Verify that this does not leak
		Gtk::Stock::lookup(Gtk::Stock::OK, Gtk::ICON_SIZE_BUTTON, *image);
		button->set_image(*image);
		add_share_file_dialog_connection = button->signal_clicked().connect(boost::bind(&ShareList::on_add_share_file_dialog_button_clicked, this));
		add_share_file_dialog.set_transient_for(_parent_window);
		add_share_file_dialog.set_modal(true);
#if GTK_CHECK_VERSION(2, 18, 3)
		add_share_file_dialog.set_create_folders(false);
#endif
	}
	{
		Gtk::Button* button;
		button = add_share_folder_dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		button->signal_clicked().connect(boost::bind(&Gtk::Widget::hide, &add_share_folder_dialog));
		button = add_share_folder_dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
		button->set_label("_Select");
		Gtk::Image* image = Gtk::manage(new Gtk::Image()); // FIXME: Verify that this does not leak
		Gtk::Stock::lookup(Gtk::Stock::OK, Gtk::ICON_SIZE_BUTTON, *image);
		button->set_image(*image);
		add_share_folder_dialog_connection = button->signal_clicked().connect(boost::bind(&ShareList::on_add_share_folder_dialog_button_clicked, this));
		add_share_folder_dialog.set_transient_for(_parent_window);
		add_share_folder_dialog.set_modal(true);
#if GTK_CHECK_VERSION(2, 18, 3)
		add_share_folder_dialog.set_create_folders(false);
#endif
	}
	{
		Gtk::Button* button;
		button = pick_download_destination_dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		button->signal_clicked().connect(boost::bind(&Gtk::Widget::hide, &pick_download_destination_dialog));
		button = pick_download_destination_dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
		button->set_label("_Select");
		Gtk::Image* image = Gtk::manage(new Gtk::Image()); // FIXME: Verify that this does not leak
		Gtk::Stock::lookup(Gtk::Stock::OK, Gtk::ICON_SIZE_BUTTON, *image);
		button->set_image(*image);
		button->signal_clicked().connect(boost::bind(&ShareList::on_pick_download_destination_dialog_button_clicked, this));
		pick_download_destination_dialog.set_transient_for(_parent_window);
		pick_download_destination_dialog.set_modal(true);
		// Unlike the two dialogs above this *is* allowed to create new folders
#if GTK_CHECK_VERSION(2, 18, 3)
		pick_download_destination_dialog.set_create_folders(true);
#endif
	}


	/* Create actions */
	Glib::RefPtr<Gtk::ActionGroup> actiongroup_ref(Gtk::ActionGroup::create("UFTT"));
	Glib::RefPtr<Gtk::Action> action; // Only used when we need to add an accel_path to a menu-item

	/* File menu */
	actiongroup_ref->add(
		Gtk::Action::create(
			"FileAddShareFile",
			Gtk::Stock::FILE,
			"Share _File",
			"Share a single file"
		),
		boost::bind(&Gtk::Dialog::present, &add_share_file_dialog)
	);
	actiongroup_ref->add(
		Gtk::Action::create(
			"FileAddShareFolder",
			Gtk::Stock::DIRECTORY,
			"Share F_older",
			"Share a whole folder"
		),
		boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog)
	);

	/* Share menu */
	action = Gtk::Action::create("ShareMenu", "S_hare");
	action->set_sensitive(false);
	actiongroup_ref->add(action);

	action = Gtk::Action::create(
		"ShareDownload",
		Gtk::Stock::GO_DOWN,
		"_Download Share",
		"Download the selected shares to the 'Download destination folder'"
	);
	action->set_sensitive(false);
	actiongroup_ref->add(
		action,
		boost::bind(&ShareList::download_selected_shares, this)
	);
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/Share/Download");

	action = Gtk::Action::create(
		"ShareDownloadTo",
		Gtk::Stock::GO_DOWN,
		"Download Share _To",
		"Download the selected shares to a newly selected folder"
	);
	action->set_sensitive(false);
	actiongroup_ref->add(
		action,
		boost::bind(&Gtk::Dialog::present, &pick_download_destination_dialog)
	);
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/Share/DownloadTo");

	action = Gtk::Action::create(
		"ShareRemoveShare",
		Gtk::Stock::REMOVE,
		"_Remove Share",
		"Removes the selected shares from the list of shares"
	);
	action->set_sensitive(false);
	actiongroup_ref->add(
		action,
		boost::bind(&ShareList::remove_selected_shares, this)
	);
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/Share/RemoveShare");

	/* View menu */
	action = Gtk::Action::create("ViewRefreshShareList",Gtk::Stock::REFRESH, "_Refresh Shares", "Refresh the list of shares");
	actiongroup_ref->add(action, boost::bind(&ShareList::on_refresh_shares, this));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/RefreshShareList");

	uimanager_ref->insert_action_group(actiongroup_ref);

	/**
	 * NOTE: The actual layout for the menu, toolbar items and pop-ups is done
	 *       in the XML description in UFTTWindow (see GTKImpl.cpp).
	 */
}

Gtk::Widget* ShareList::get_mnemonic_widget() {
	return &share_list_treeview;
}

void ShareList::on_refresh_shares() {
	// FIXME: begin QTGui QuirkMode Emulation TM
	Gdk::ModifierType mask;
	int x,y;
	get_window()->get_pointer(x, y, mask);
	if((mask & Gdk::SHIFT_MASK) != Gdk::SHIFT_MASK) {
		share_list_liststore->clear();
	}
	if((mask & Gdk::CONTROL_MASK) != Gdk::CONTROL_MASK) {
		for(int i=0; i<8; ++i) {
			Glib::signal_timeout().connect_once(
				dispatcher.wrap(
					boost::function<void(void)>(
						boost::lambda::if_then(
							boost::lambda::var(core),
							boost::lambda::bind(&UFTTCore::doRefreshShares, core)
						)
					)
				),
				i*20
			);
		}
	}
}

void ShareList::on_add_share_file_dialog_button_clicked() {
	add_share_file_dialog_connection.block();  // Work around some funny Gtk behaviour
	std::string filename = add_share_file_dialog.get_filename();
	if(filename != "") {
		ext::filesystem::path path = filename;
		if(ext::filesystem::exists(path)) {
			if(boost::filesystem::is_directory(path)) {
				add_share_file_dialog.set_current_folder(filename);
			}
			else {
				add_share_file_dialog.hide();
				core->addLocalShare(path);
			}
		}
		else {
			Gtk::MessageDialog dialog(*(Gtk::Window*)get_toplevel(), "File does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dialog.set_transient_for(add_share_file_dialog);
			dialog.set_modal(true);
			dialog.set_secondary_text("The file you have selected to share does not appear to exist.\nPlease verify that the path and filename are correct and try again.");
			dialog.run();
		}
	}
	add_share_file_dialog_connection.unblock();
}

void ShareList::on_add_share_folder_dialog_button_clicked() {
	add_share_folder_dialog_connection.block(); // Work around some funny Gtk behaviour
	std::string filename = add_share_folder_dialog.get_filename();
	if(filename != "") {
		ext::filesystem::path path = filename;
		if(ext::filesystem::exists(path)) {
			add_share_folder_dialog.hide();
			core->addLocalShare(path);
		}
		else {
			Gtk::MessageDialog dialog(*(Gtk::Window*)get_toplevel(), "Folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dialog.set_transient_for(add_share_folder_dialog);
			dialog.set_modal(true);
			dialog.set_secondary_text("The folder you have selected to share does not appear to exist.\nPlease verify that the path and foldername are correct and try again.");
			dialog.run();
		}
	}
	add_share_folder_dialog_connection.unblock();
}

void ShareList::on_share_list_treeview_selection_signal_changed() {
	bool has_selection = share_list_treeview.get_selection()->count_selected_rows() > 0;
	uimanager_ref->get_action("/MenuBar/ShareMenu")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/ShareMenu/ShareDownload")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/ShareMenu/ShareDownloadTo")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/ShareMenu/ShareRemoveShare")->set_sensitive(has_selection);
}

bool ShareList::on_share_list_treeview_signal_key_press_event(GdkEventKey* event) {
	/**
	 * We 'disconnect' the normal handlers so that we can prevent the
	 * the 'row-activated' handler from running upon pressing 'enter'.
	 * This is because the default row-activated handler reduces the
	 * selection to a single item before firing the 'row-activated'
	 * signal.
	 */
	if(event->type == GDK_KEY_PRESS) {
		switch(event->keyval) {
			case GDK_KEY_Return:
			case GDK_KEY_KP_Enter: {
				download_selected_shares();
				return true;
			}; break;
		}
	}
	return false;
}

bool ShareList::on_share_list_treeview_signal_button_press_event(GdkEventButton* event) {
	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		Gtk::TreeModel::Path  path;
		Gtk::TreeViewColumn* column;
		int    cell_x, cell_y;
		bool exists = share_list_treeview.get_path_at_pos((int)(event->x), (int)(event->y), path, column, cell_x, cell_y);
		if(exists) {
			if(!share_list_treeview.get_selection()->is_selected(path)) { // Clicked on a tree row, but it is not selected
				share_list_treeview.get_selection()->unselect_all();
				share_list_treeview.get_selection()->select(path);
			}
		}

		((Gtk::Menu*)uimanager_ref->get_widget("/ShareListPopup"))->popup(event->button, event->time);
		return true;
	}
	return false;
}

void ShareList::set_backend(UFTTCore* _core) {
	core = _core;
	core->connectSigAddShare(dispatcher.wrap(boost::bind(&ShareList::on_signal_add_share, this, _1)));
}

void ShareList::on_download_destination_path_entry_signal_changed() {
	ext::filesystem::path dl_path(download_destination_path_entry.get_text());
	while(dl_path.filename() == ".") dl_path = dl_path.parent_path(); // Remove trailing '/' 's

	settings->dl_path = dl_path;
	#ifdef USE_GTK24_API
		#define override_color(color, state)            modify_text(state, color)
		#define override_background_color(color, state) modify_base(state, color)
		#define unset_color                             unset_text
		#define unset_background_color                  unset_base
		#define RGBA                                    Color
		#define STATE_FLAG_NORMAL                       STATE_NORMAL
	#endif
	if(!ext::filesystem::exists(dl_path)) {
		download_destination_path_entry.override_color(Gdk::RGBA("#000000"), Gtk::STATE_FLAG_NORMAL);
		// Note: some windows app uses #ffb3b3 and black text, anjuta uses #ff6666 and white text (the average is about #ff9494)
		download_destination_path_entry.override_background_color(Gdk::RGBA("#ffb3b3"), Gtk::STATE_FLAG_NORMAL);
	}
	else {
		download_destination_path_entry.unset_color(Gtk::STATE_FLAG_NORMAL);
		download_destination_path_entry.unset_background_color(Gtk::STATE_FLAG_NORMAL);
		on_download_destination_path_entry_signal_changed_connection.block();
		browse_for_download_destination_path_button.set_current_folder(dl_path.string());
	}
	#ifdef USE_GTK24_API
		#undef override_color
		#undef override_background_color
		#undef unset_color
		#undef unset_background_color
		#undef RGBA
		#undef STATE_FLAG_NORMAL
	#endif
}

void ShareList::on_browse_for_download_destination_path_button_signal_current_folder_changed() {
	ext::filesystem::path pa(download_destination_path_entry.get_text());
	ext::filesystem::path pb(browse_for_download_destination_path_button.get_current_folder());
	while(pa.filename() == ".") pa = pa.parent_path();
	while(pb.filename() == ".") pb = pb.parent_path();

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
	size_t begin = urilist.find("file://", 0);
	size_t end = urilist.find("\r\n", begin);
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
				ext::filesystem::path path = file;
				core->addLocalShare(path);
			}
		}; break;
		case 2:
		case 1: {
				ext::filesystem::path path = selection_data.get_data_as_string();
				core->addLocalShare(path);
		}; break;
		default:
			cout << "Warning: unhandled drop event!" << endl;
			context->drag_finish(false, false, time);
			return;
	}
	context->drag_finish(true, false, time);
}

void ShareList::on_pick_download_destination_dialog_button_clicked() {
	std::string filename = pick_download_destination_dialog.get_filename();
	if(filename != "") {
		ext::filesystem::path path = filename;
		pick_download_destination_dialog.hide();
		download_selected_shares(path);
	}
}

void ShareList::download_selected_shares() {
	download_selected_shares(settings->dl_path);
}

void ShareList::download_selected_shares(ext::filesystem::path destination) {
	//FIXME: Don't forget to test dl_path for validity and writeablity
	if(!ext::filesystem::exists(destination)) {
		Gtk::MessageDialog dialog(*(Gtk::Window*)get_toplevel(), "Download destination folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text("The folder you have selected for downloaded shares to be placed in does not appear to exist.\nPlease select another download destination and try again.");
		dialog.set_transient_for(*(Gtk::Window*)get_toplevel());
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
		core->startDownload(sid, destination);
	}
}

void ShareList::remove_selected_shares() {
	Glib::RefPtr<Gtk::TreeSelection> sel = share_list_treeview.get_selection();
	BOOST_FOREACH(Gtk::TreeModel::Path p, sel->get_selected_rows()) {
		const Gtk::TreeModel::Row& row = *(share_list_liststore->get_iter(p));
		LocalShareID id;
		ShareID sid = row[share_list_columns.share_id];
		if(core->getLocalShareID((Glib::ustring)row[share_list_columns.share_name], &id)) {
			core->delLocalShare(id);
		}
	}
	// FIXME: Should not be neccessary, we should get a notification
	//        from the core/backend
	share_list_liststore->clear();
	core->doRefreshShares();
}

void ShareList::on_signal_add_share(const ShareInfo& info) {
	if (info.isupdate) { // FIXME: Should be handled by core
		return;
	}

	uint32 version = atoi(info.proto.substr(6).c_str()); // FIXME: Perhaps put this type of check in the core
	bool found = false;
	BOOST_FOREACH(const Gtk::TreeRow& row, share_list_liststore->children()) {
		if(((std::string)(Glib::ustring)row[share_list_columns.host_name] == info.host) && ((std::string)(Glib::ustring)row[share_list_columns.share_name] == info.name)) {
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

		// If this is the first share to be added, automatically select it.
		if(share_list_liststore->children().size() == 1) {
			share_list_treeview.get_selection()->select(i);
		}
	}
}
