#include "GTKImpl.h"
#include "OStreamGtkTextBuffer.h"
#include "../util/StrFormat.h"
#include "../util/Filesystem.h"
#include <ios>
#include <boost/bind.hpp>
#include <glib/gthread.h>
#include <gtkmm/stock.h>
#include <gtkmm/action.h>
#include <gtkmm/liststore.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/treemodelsort.h>
#include <gtkmm/treeviewcolumn.h>

using namespace std;

UFTTWindow::UFTTWindow(UFTTSettingsRef _settings) 
: settings(_settings),
  browse_for_download_destination_path_button(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  refresh_shares_toolbutton(Gtk::Stock::REFRESH),
  download_shares_toolbutton(Gtk::Stock::EXECUTE),
  edit_preferences_toolbutton(Gtk::Stock::PREFERENCES)
{
	signal_hide().connect(boost::bind(&UFTTWindow::on_signal_hide, this));
	set_title("UFTT");
	set_default_size(1024, 640);
	set_position(Gtk::WIN_POS_CENTER);

	/*
	guint8* data = new guint8[gmpmpc_icon_data.width * gmpmpc_icon_data.height * gmpmpc_icon_data.bytes_per_pixel];
	GIMP_IMAGE_RUN_LENGTH_DECODE(data,
		                           gmpmpc_icon_data.rle_pixel_data,
		                           gmpmpc_icon_data.width * gmpmpc_icon_data.height,
		                           gmpmpc_icon_data.bytes_per_pixel);

	statusicon_pixbuf = Gdk::Pixbuf::create_from_data(data,
		                                                Gdk::COLORSPACE_RGB,
		                                                true,
		                                                8,
		                                                256,
		                                                256,
		                                                256*4
		                                                );
	statusicon = Gtk::StatusIcon::create(statusicon_pixbuf);
	set_default_icon(statusicon_pixbuf);
	statusicon->set_tooltip("UFTT\nThe Ultimate File Transfer Tool");
	statusicon->set_visible(true);
	*/

	/* Create actions */
	m_refActionGroup = Gtk::ActionGroup::create();
	/* File menu */
	m_refActionGroup->add(Gtk::Action::create("FileMenu", "_File"));
	m_refActionGroup->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
	                      sigc::mem_fun(*this, &UFTTWindow::on_menu_file_quit));

	m_refActionGroup->add(Gtk::Action::create("ViewMenu", "_View"));
	m_refActionGroup->add(Gtk::ToggleAction::create("ViewSharelist", "_Sharelist"));
	m_refActionGroup->add(Gtk::ToggleAction::create("ViewDebuglog", "_Debuglog"));
	m_refActionGroup->add(Gtk::ToggleAction::create("ViewManualConnect", "_Manual Connect"));
	m_refActionGroup->add(Gtk::ToggleAction::create("ViewTasklist", "_Tasklist"));

	m_refActionGroup->add(Gtk::Action::create("OptionsMenu", "_Options"));
	m_refActionGroup->add(Gtk::ToggleAction::create("OptionsEnableGlobalPeerDiscovery", "Enable _global peer discovery"));
	m_refActionGroup->add(Gtk::ToggleAction::create("OptionsEnableDownloadResume", "Enable _download resume"));
	m_refActionGroup->add(Gtk::ToggleAction::create("OptionsEnableAutomaticUpdatesFromPeers", "Enable automatic _updates from peers"));
	m_refActionGroup->add(Gtk::Action::create("OptionsCheckForWebUpdatesNow", "Check for _web-updates now"));	
	m_refActionGroup->add(Gtk::Action::create("OptionsCheckForUpdatesAutomatically", "Check for _updates automatically"));	
	Gtk::RadioButtonGroup& rbg(menu_options_check_updates_frequency_radio_button_group);
	m_refActionGroup->add(Gtk::RadioAction::create(rbg, "OptionsCheckForUpdatesAutomaticallyNever", "_Never"));	
	m_refActionGroup->add(Gtk::RadioAction::create(rbg, "OptionsCheckForUpdatesAutomaticallyDaily", "_Daily"));	
	m_refActionGroup->add(Gtk::RadioAction::create(rbg, "OptionsCheckForUpdatesAutomaticallyWeekly", "_Weekly"));	
	m_refActionGroup->add(Gtk::RadioAction::create(rbg, "OptionsCheckForUpdatesAutomaticallyMonthly", "_Monthly"));	

	m_refActionGroup->add(Gtk::Action::create("Help", "_Help"));	
	m_refActionGroup->add(Gtk::Action::create("HelpAboutUFTT", "About _UFTT"));	
	m_refActionGroup->add(Gtk::Action::create("HelpAboutGTK", "About _GTK"));	

	m_refUIManager = Gtk::UIManager::create();
	m_refUIManager->insert_action_group(m_refActionGroup);

	add_accel_group(m_refUIManager->get_accel_group());

	/* Layout the actions */
	Glib::ustring ui_info = "<ui>"
	                        "  <menubar name='MenuBar'>"
	                        "    <menu action='FileMenu'>"
	                        "      <menuitem action='FileQuit'/>"
	                        "    </menu>"
	                        "    <menu action='ViewMenu'>"
	                        "      <menuitem action='ViewSharelist'/>"
	                        "      <menuitem action='ViewDebuglog'/>"
	                        "      <menuitem action='ViewManualConnect'/>"
	                        "      <menuitem action='ViewTasklist'/>"
	                        "    </menu>"
	                        "    <menu action='OptionsMenu'>"
	                        "      <menuitem action='OptionsEnableGlobalPeerDiscovery'/>"
	                        "      <menuitem action='OptionsEnableDownloadResume'/>"
	                        "      <separator/>"
	                        "      <menuitem action='OptionsEnableAutomaticUpdatesFromPeers'/>"
	                        "      <menuitem action='OptionsCheckForWebUpdatesNow'/>"
	                        "      <menu action='OptionsCheckForUpdatesAutomatically'>"
	                        "        <menuitem action='OptionsCheckForUpdatesAutomaticallyNever'/>"
	                        "        <menuitem action='OptionsCheckForUpdatesAutomaticallyDaily'/>"
	                        "        <menuitem action='OptionsCheckForUpdatesAutomaticallyWeekly'/>"
	                        "        <menuitem action='OptionsCheckForUpdatesAutomaticallyMonthly'/>"
	                        "      </menu>"
	                        "    </menu>"
	                        "    <menu action='Help'>"
	                        "      <menuitem action='HelpAboutUFTT'/>"
	                        "      <menuitem action='HelpAboutGTK'/>"
	                        "    </menu>"
	                        "  </menubar>"
	                        "</ui>";

	m_refUIManager->add_ui_from_string(ui_info); //FIXME: This may throw an error if the XML above is not valid
	menubar_ptr = (Gtk::Menu*)m_refUIManager->get_widget("/MenuBar");

	share_list_frame.set_label("Sharelist:");
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
	}
	share_list_treeview.set_enable_search(true);
	share_list_treeview.set_rubber_banding(true);
	share_list_treeview.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	std::list<Gtk::TargetEntry> listTargets;
	listTargets.push_back( Gtk::TargetEntry("text/uri-list", Gtk::TARGET_OTHER_APP, 0) ); // Last parameter is 'Info', used to distinguish
	listTargets.push_back( Gtk::TargetEntry("text/plain"   , Gtk::TARGET_OTHER_APP, 1) ); // different types of TargetEntry in the drop handler
	listTargets.push_back( Gtk::TargetEntry("STRING"       , Gtk::TARGET_OTHER_APP, 2) );
	share_list_treeview.drag_dest_set(listTargets); // Should use defaults, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
	share_list_treeview.signal_drag_data_received().connect(boost::bind(&UFTTWindow::on_share_list_treeview_signal_drag_data_received, this, _1, _2, _3, _4, _5, _6));
	share_list_scrolledwindow.add(share_list_treeview);
	share_list_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	on_download_destination_path_entry_signal_changed_connection =
		download_destination_path_entry.signal_changed().connect(
			boost::bind(&UFTTWindow::on_download_destination_path_entry_signal_changed, this));
	download_destination_path_entry.set_text(settings->dl_path.native_directory_string());
	download_destination_path_hbox.add(download_destination_path_entry);
	download_destination_path_hbox.pack_start(browse_for_download_destination_path_button, Gtk::PACK_SHRINK);
	browse_for_download_destination_path_button.signal_current_folder_changed().connect( // FIXME: Dialog is not modal
		boost::bind(&UFTTWindow::on_browse_for_download_destination_path_button_signal_current_folder_changed, this));
	download_destination_path_label.set_alignment(0.0f, 0.5f);
	download_destination_path_label.set_text("Download location:");
	download_destination_path_vbox.add(download_destination_path_label);
	download_destination_path_vbox.add(download_destination_path_hbox);
	download_destination_path_alignment.add(download_destination_path_vbox);
	download_destination_path_alignment.set_padding(8, 4, 4, 4);
	share_list_alignment.set_padding(4, 4, 4, 4);
	task_list_alignment.set_padding(4, 4, 4, 4);
	debug_log_alignment.set_padding(4, 4, 4, 4);

	
	share_list_vbox.add(share_list_scrolledwindow);
	share_list_vbox.pack_start(download_destination_path_alignment, Gtk::PACK_SHRINK);
	share_list_frame.add(share_list_vbox);
	share_list_alignment.add(share_list_frame);
	share_task_list_vpaned.add(share_list_alignment);
	task_list_frame.set_label("Tasklist:");
	task_list_frame.add(task_list_treeview);
	task_list_alignment.add(task_list_frame);
	share_task_list_vpaned.add(task_list_alignment);
	main_paned.add(share_task_list_vpaned);
	debug_log_textview.set_buffer(Glib::RefPtr<Gtk::TextBuffer>(new OStreamGtkTextBuffer(std::cout)));
	debug_log_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	debug_log_scrolledwindow.add(debug_log_textview);
	debug_log_frame.add(debug_log_scrolledwindow);
	debug_log_frame.set_label("Debuglog:");
	debug_log_alignment.add(debug_log_frame);
	main_paned.add(debug_log_alignment);
	menu_main_paned_vbox.pack_start(*menubar_ptr, Gtk::PACK_SHRINK);
	refresh_shares_toolbutton.set_label("Refresh");
	download_shares_toolbutton.set_label("Download");
	edit_preferences_toolbutton.set_label("Edit Preferences");
	toolbar.append(download_shares_toolbutton);
	toolbar.append(refresh_shares_toolbutton);
	toolbar.append(refresh_preferences_separatortoolitem);
	toolbar.append(edit_preferences_toolbutton);
	menu_main_paned_vbox.pack_start(toolbar, Gtk::PACK_SHRINK);
	menu_main_paned_vbox.add(main_paned);
	add(menu_main_paned_vbox);

	// FIXME: We need to call show_all() and present() here, otherwise some
	//         widgets will not know their own size and setting the positions
	//         of the paneds will fail. However this leads to an ugly presentation
	//         to the user because the user can now see this layouting process
	//         in action.
	show_all();
	present();
	share_task_list_vpaned.set_position(share_task_list_vpaned.get_height()/2);
	main_paned.set_position(main_paned.get_width()*5/8);
}

void UFTTWindow::on_download_destination_path_entry_signal_changed() {
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

void UFTTWindow::on_browse_for_download_destination_path_button_signal_current_folder_changed() {
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

void UFTTWindow::on_share_list_treeview_signal_drag_data_received(
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

void UFTTWindow::add_share(const ShareInfo& info) {
	Gtk::TreeModel::iterator i = share_list_liststore->append();
	(*i)[share_list_columns.user_name]  = info.user;
	(*i)[share_list_columns.share_name] = info.name;
	if(info.name == "") (*i)[share_list_columns.share_name] = "Anonymous";
	(*i)[share_list_columns.host_name]  = info.host;
	(*i)[share_list_columns.protocol]   = info.proto;
	(*i)[share_list_columns.url]        = STRFORMAT("%s:\\\\%s\\%s", info.proto, info.host, info.name);
}

void UFTTWindow::set_backend(UFTTCoreRef _core) {
	dispatcher.invoke(boost::bind(&UFTTWindow::_set_backend, this, _core));
}

void UFTTWindow::_set_backend(UFTTCoreRef _core) {
	if(_core->error_state == 2) { // FIXME: Magic Constant
		Gtk::MessageDialog dialog("Fatal error.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text(STRFORMAT("There was a problem during the initialization of UFTT's core:\n\n\"%s\"\n\nUFTT can not continue and will now quit.", _core->error_string));
		dialog.run();
//		throw int(1); // thrown integers will quit application with integer as exit code // FIXME: Can't we just quit() nicely?
		on_menu_file_quit();
	}
	if(_core->error_state == 1) {// FIXME: Magic Constant
		Gtk::MessageDialog dialog("Warning.", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
		dialog.set_secondary_text(STRFORMAT("%s\n\nDo you still want to continue?", _core->error_string));
		dialog.set_default_response(Gtk::RESPONSE_YES);
		if(dialog.run() == Gtk::RESPONSE_NO) {
			on_menu_file_quit();
		}
	}

	this->core = _core;
	core->connectSigAddShare(dispatcher.wrap(boost::bind(&UFTTWindow::add_share, this, _1)));
}

void UFTTWindow::on_signal_hide() { // Close button (?)
	on_menu_file_quit();
}

void UFTTWindow::on_menu_file_quit() {
	Gtk::Main::quit();
}
