#include "GTKImpl.h"
#include "OStreamGtkTextBuffer.h"
#include "../util/StrFormat.h"
#include "../util/Filesystem.h"
#include "uftt-16x16.png.h"
//#include "uftt-22x22.png.h" // FIXME: Does not exist (yet)
#include "uftt-32x32.png.h"
#include "uftt-48x48.png.h"
#include <ios>
#include <boost/bind.hpp>
#include <glib/gthread.h>
#include <gdkmm/pixbufloader.h>
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
  add_share_file_dialog(*this, "Select a file", Gtk::FILE_CHOOSER_ACTION_OPEN),
  add_share_folder_dialog(*this, "Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  browse_for_download_destination_path_button("Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  refresh_shares_toolbutton(Gtk::Stock::REFRESH),
  download_shares_toolbutton(Gtk::Stock::EXECUTE),
  edit_preferences_toolbutton(Gtk::Stock::PREFERENCES),
  add_share_file_toolbutton(Gtk::Stock::FILE),
  add_share_folder_toolbutton(Gtk::Stock::DIRECTORY)
{
	set_title("UFTT");
	set_default_size(1024, 640);
	set_position(Gtk::WIN_POS_CENTER);

	{
		Gtk::Button* button;
		button = add_share_file_dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		button->signal_clicked().connect(boost::bind(&Gtk::Widget::hide, &add_share_file_dialog));
		button = add_share_file_dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
		button->set_label("_Select");
		Gtk::Image* image = Gtk::manage(new Gtk::Image()); // FIXME: Verify that this does not leak
		Gtk::Stock::lookup(Gtk::Stock::OK, Gtk::ICON_SIZE_BUTTON, *image);
		button->set_image(*image);
		add_share_file_dialog_connection = button->signal_clicked().connect(boost::bind(&UFTTWindow::on_add_share_file, this));
		add_share_file_dialog.set_transient_for(*this);
		add_share_file_dialog.set_modal(true);
		add_share_file_dialog.set_create_folders(false);
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
		add_share_folder_dialog_connection = button->signal_clicked().connect(boost::bind(&UFTTWindow::on_add_share_folder, this));
		add_share_folder_dialog.set_transient_for(*this);
		add_share_folder_dialog.set_modal(true);
		add_share_folder_dialog.set_create_folders(false);
	}

	statusicon_pixbuf = get_best_uftt_icon_for_size(256, 256);
	statusicon = Gtk::StatusIcon::create(statusicon_pixbuf);
	set_default_icon(statusicon_pixbuf);
	statusicon->set_tooltip("UFTT\nThe Ultimate File Transfer Tool");
	statusicon->set_visible(true);
	sigc::slot<bool, int> slot = sigc::mem_fun(*this, &UFTTWindow::on_statusicon_signal_size_changed);
	statusicon->signal_size_changed().connect(slot);
	statusicon->signal_popup_menu().connect(boost::bind(&UFTTWindow::on_statusicon_signal_popup_menu, this, _1, _2));
	statusicon->signal_activate().connect(boost::bind(&UFTTWindow::on_statusicon_signal_activate, this));

	std::vector<Glib::RefPtr<Gdk::Pixbuf> > icon_list;
	icon_list.push_back(get_best_uftt_icon_for_size(16, 16));
//	icon_list.push_back(get_best_uftt_icon_for_size(22, 22));
	icon_list.push_back(get_best_uftt_icon_for_size(32, 32));
	icon_list.push_back(get_best_uftt_icon_for_size(48, 48));
	set_default_icon_list(icon_list);


	/* Create actions */
	m_refActionGroup = Gtk::ActionGroup::create();
	/* File menu */
	m_refActionGroup->add(Gtk::Action::create("FileMenu", "_File"));
	m_refActionGroup->add(Gtk::Action::create("FileAddShareFile", Gtk::Stock::FILE, "Share file"),
	                      boost::bind(&Gtk::Dialog::present, &add_share_file_dialog));
	m_refActionGroup->add(Gtk::Action::create("FileAddSharefolder", Gtk::Stock::DIRECTORY, "Share folder"),
	                      boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog));
	m_refActionGroup->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
	                      boost::bind(&UFTTWindow::on_menu_file_quit, this));

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
	m_refActionGroup->add(Gtk::Action::create("HelpAboutUFTT", Gtk::Stock::ABOUT, "About _UFTT"));	
	m_refActionGroup->add(Gtk::Action::create("HelpAboutGTK", Gtk::Stock::ABOUT, "About _GTK"));	

	m_refUIManager = Gtk::UIManager::create();
	m_refUIManager->insert_action_group(m_refActionGroup);

	add_accel_group(m_refUIManager->get_accel_group());

	/* Layout the actions */
	Glib::ustring ui_info = "<ui>"
	                        "  <menubar name='MenuBar'>"
	                        "    <menu action='FileMenu'>"
	                        "      <menuitem action='FileAddShareFile'/>"
	                        "      <menuitem action='FileAddSharefolder'/>"
	                        "      <separator/>"
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
	                        "  <popup name='StatusIconPopup'>"
	                        "    <menuitem action='FileAddShareFile'/>"
	                        "    <menuitem action='FileAddSharefolder'/>"
	                        "    <separator/>"
	                        "    <menuitem action='FileQuit'/>"
	                        "  </popup>"
	                        "</ui>";

	m_refUIManager->add_ui_from_string(ui_info); //FIXME: This may throw an error if the XML above is not valid
	menubar_ptr = (Gtk::Menu*)m_refUIManager->get_widget("/MenuBar");

	/* Begin Share List */
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
	share_list_treeview.signal_drag_data_received().connect(boost::bind(&UFTTWindow::on_share_list_treeview_signal_drag_data_received, this, _1, _2, _3, _4, _5, _6));
	// FIXME: TreeView deselects rows before calling activated when pressing enter.
	//         See http://bugzilla.xfce.org/show_bug.cgi?id=5943
	share_list_treeview.signal_row_activated().connect(boost::bind(&UFTTWindow::download_selected_shares, this));
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
	download_destination_path_label.set_text("Download destination folder:");
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
	/* End Share List */

	/* Begin Task List */
	task_list_liststore = Gtk::ListStore::create(task_list_columns);
//	task_list_treeview.set_model(SortableTreeDragDest<Gtk::ListStore>::create(task_list_liststore)); // FIXME: Enabling this causes Gtk to give a silly warning
	task_list_treeview.set_model(task_list_liststore);
	#define ADD_TV_COL_SORTABLE(tv, title, column) tv.get_column(tv.append_column(title, column) - 1)->set_sort_column(column);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Status"         , task_list_columns.status);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Time Elapsed"   , task_list_columns.time_elapsed);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Time Remaining" , task_list_columns.time_remaining);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Transferred"    , task_list_columns.transferred);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Total Size"     , task_list_columns.total_size);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Speed"          , task_list_columns.speed);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Queue"          , task_list_columns.queue);
	ADD_TV_COL_SORTABLE(task_list_treeview, "User Name"      , task_list_columns.user_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Share Name"     , task_list_columns.share_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Host Name"      , task_list_columns.host_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Protocol"       , task_list_columns.protocol);
	ADD_TV_COL_SORTABLE(task_list_treeview, "URL"            , task_list_columns.url);
	#undef ADD_TV_COL_SORTABLE
	task_list_treeview.set_headers_clickable(true);
	task_list_treeview.set_rules_hint(true);
	task_list_treeview.set_search_column(task_list_columns.share_name);
	task_list_liststore->set_sort_column(task_list_columns.share_name, Gtk::SORT_ASCENDING);
	BOOST_FOREACH(Gtk::TreeViewColumn* column, task_list_treeview.get_columns()) {
		column->set_reorderable(true);
		column->set_resizable(true);
	}
	task_list_treeview.set_enable_search(true);
	task_list_treeview.set_rubber_banding(true);
	task_list_treeview.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	task_list_scrolledwindow.add(task_list_treeview);
	task_list_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	task_list_frame.set_label("Tasklist:");
	task_list_frame.add(task_list_scrolledwindow);
	task_list_alignment.add(task_list_frame);
	/* End Task List*/
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

	Gtk::SeparatorToolItem* separator;
	refresh_shares_toolbutton.set_label("Refresh");
	download_shares_toolbutton.set_label("Download");
	edit_preferences_toolbutton.set_label("Preferences");
	add_share_file_toolbutton.set_label("Share file");
	add_share_folder_toolbutton.set_label("Share folder");
	download_shares_toolbutton.signal_clicked().connect(boost::bind(&UFTTWindow::download_selected_shares, this));
	refresh_shares_toolbutton.signal_clicked().connect(boost::bind(&UFTTWindow::on_refresh_shares_toolbutton_clicked, this));
	add_share_file_toolbutton.signal_clicked().connect(boost::bind(&Gtk::Dialog::present, &add_share_file_dialog));
	add_share_folder_toolbutton.signal_clicked().connect(boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog));
	toolbar.append(add_share_file_toolbutton);
	toolbar.append(add_share_folder_toolbutton);
	separator = Gtk::manage(new Gtk::SeparatorToolItem());
	toolbar.append(*separator);
	toolbar.append(download_shares_toolbutton);
	toolbar.append(refresh_shares_toolbutton);
	separator = Gtk::manage(new Gtk::SeparatorToolItem());
	toolbar.append(*separator);
	toolbar.append(edit_preferences_toolbutton);
	menu_main_paned_vbox.pack_start(toolbar, Gtk::PACK_SHRINK);
	menu_main_paned_vbox.add(main_paned);
	add(menu_main_paned_vbox);

	//FIXME: View -> Toolbar on/off
	download_destination_path_entry.set_tooltip_text("Enter the folder where downloaded shares should be placed");
	browse_for_download_destination_path_button.set_tooltip_text("Select the folder where downloaded shares should be placed");
	download_shares_toolbutton.set_tooltip_text("Download selected shares");
	edit_preferences_toolbutton.set_tooltip_text("Edit Preferences");
	refresh_shares_toolbutton.set_tooltip_text("Refresh sharelist");
	add_share_file_toolbutton.set_tooltip_text("Share a single file");
	add_share_folder_toolbutton.set_tooltip_text("Share a whole folder");
	
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

void UFTTWindow::on_statusicon_signal_activate() {
	if( is_visible() ) {
		if( property_is_active() ) {
			hide();
		}
		else {
			present();
		}
	}
	else {
		show();
	}
}

void UFTTWindow::on_statusicon_signal_popup_menu(guint button, guint32 activate_time) {
	Gtk::Menu* m = (Gtk::Menu*)(m_refUIManager->get_widget("/StatusIconPopup"));
	statusicon->popup_menu_at_position(*m, button, activate_time);
}

bool UFTTWindow::on_statusicon_signal_size_changed(int xy) {
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = get_best_uftt_icon_for_size(xy, xy);
	statusicon->set(pixbuf->scale_simple(xy, xy, Gdk::INTERP_HYPER));
	return true;
}

Glib::RefPtr<Gdk::Pixbuf> UFTTWindow::get_best_uftt_icon_for_size(int x, int y) {
	long surface = x * y;
	int	best_match = 0; // starts at 48 x 48, does not allow upscaling (in either direction)
	if(x <= 32  && y <= 32) best_match = 1;
//	if(x < 22  && y < 22) best_match = 2;
	if(x <= 16  && y <= 16) best_match = 3;

	// Note we use the PixbufLoader instead of directly encoding the png with
	// gdk-pixbuf-csource in order to avoid having to find out how to access
	// gdk-pixbuf-csource from cmake
	Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create("png");
	switch(best_match) {
		default:
		case 0: {
			loader->write(uftt_48x48_png, sizeof(uftt_48x48_png));
		}; break;
		case 1: {
			loader->write(uftt_32x32_png, sizeof(uftt_32x32_png));
		}; break;
//		case 2: {
//			loader->write(uftt_48x48_png, sizeof(uftt_48x48_png));
//		}; break;
		case 3: {
			loader->write(uftt_16x16_png, sizeof(uftt_16x16_png));
		}; break;
	}
	loader->close();
	return loader->get_pixbuf();
}

void UFTTWindow::on_add_share_file() {
	add_share_file_dialog_connection.block();  // Work around some funny Gtk behaviour
	std::string filename = add_share_file_dialog.get_filename();
	if(filename != "") {
		boost::filesystem::path path = filename;
		if(ext::filesystem::exists(path)) {
			add_share_file_dialog.hide();
			core->addLocalShare(path.leaf(), path);
		}
		else {
			Gtk::MessageDialog dialog("File does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dialog.set_transient_for(add_share_file_dialog);
			dialog.set_modal(true);
			dialog.set_secondary_text("The file you have selected to share does not appear to exist.\nPlease verify that the path and filename are correct and try again.");
			dialog.run();
		}
	}
	add_share_file_dialog_connection.unblock();
}

void UFTTWindow::on_add_share_folder() {
	add_share_folder_dialog_connection.block(); // Work around some funny Gtk behaviour
	std::string filename = add_share_folder_dialog.get_filename();
	if(filename != "") {
		boost::filesystem::path path = filename;
		if(ext::filesystem::exists(path)) {
			add_share_folder_dialog.hide();
			core->addLocalShare(path.leaf(), path);
		}
		else {
			Gtk::MessageDialog dialog("Folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dialog.set_transient_for(add_share_folder_dialog);
			dialog.set_modal(true);
			dialog.set_secondary_text("The folder you have selected to share does not appear to exist.\nPlease verify that the path and foldername are correct and try again.");
			dialog.run();
		}
	}
	add_share_folder_dialog_connection.unblock();
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

void UFTTWindow::download_selected_shares() {
	//FIXME: Don't forget to test dl_path for validity and writeablity
	if(!ext::filesystem::exists(settings->dl_path)) {
		Gtk::MessageDialog dialog("Download destination folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text("The folder you have selected for downloaded shares to be placed in does not appear to exist.\nPlease select another download destination and try again.");
		dialog.set_transient_for(*this);
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
		core->addDownloadTask(sid, settings->dl_path);
	}
}

bool UFTTWindow::refresh_shares() {
	if(core) core->doRefreshShares();
	return false;
}

void UFTTWindow::on_refresh_shares_toolbutton_clicked() {
	// FIXME: begin QTGui QuirkMode Emulation TM
	Gdk::ModifierType mask;
	int x,y;
	get_window()->get_pointer(x, y, mask);
	if((mask & Gdk::SHIFT_MASK) != Gdk::SHIFT_MASK) {
		share_list_liststore->clear();
	}
	if((mask & Gdk::CONTROL_MASK) != Gdk::CONTROL_MASK) {
		for(int i=0; i<8; ++i) {
			sigc::slot<bool> slot = sigc::mem_fun(*this, &UFTTWindow::refresh_shares);
			Glib::signal_timeout().connect(slot, i*20);
		}
	}
}

void UFTTWindow::on_signal_add_share(const ShareInfo& info) {
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
				row[share_list_columns.url]       = STRFORMAT("%s:\\\\%s\\%s", info.proto, info.host, info.name);
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
		(*i)[share_list_columns.url]        = STRFORMAT("%s:\\\\%s\\%s", info.proto, info.host, info.name);
		(*i)[share_list_columns.share_id]   = info.id;
	}
}

void UFTTWindow::on_signal_task_status(const Gtk::TreeModel::iterator i, const boost::posix_time::ptime start_time, const TaskInfo& info) {
	boost::posix_time::ptime current_time = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration time_elapsed = current_time-start_time;

	(*i)[task_list_columns.status]         = info.status;
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(time_elapsed);
	if (info.size > 0 && info.size >= info.transferred && info.transferred > 0) {
		(*i)[task_list_columns.time_remaining] = 
			boost::posix_time::to_simple_string(
				boost::posix_time::time_duration(
					boost::posix_time::seconds(
						((info.size-info.transferred) * time_elapsed.total_seconds() / info.transferred)
					)
				)
			);
	}
	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	if(time_elapsed.total_seconds() > 0) {
		(*i)[task_list_columns.speed] = STRFORMAT("%s\\s", StrFormat::bytes(info.transferred/time_elapsed.total_seconds()));
	}
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user;
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.share_name]     = (info.isupload ? "U: " : "D: ") + info.shareinfo.name;

// FIXME: Something with auto-update
//	if (!info.isupload && info.status == "Completed") {} 
}

void UFTTWindow::on_signal_new_task(const TaskInfo& info) {
	Gtk::TreeModel::iterator i = task_list_liststore->append();
	// Task info (real values only filled in on_signal_task_status)
	(*i)[task_list_columns.status]         = "Waiting for peer";
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(boost::posix_time::time_duration(boost::posix_time::seconds(0)));
	(*i)[task_list_columns.time_remaining] = "Unknown";
	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	(*i)[task_list_columns.speed]          = "Unknown";
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user;
	(*i)[task_list_columns.share_name]     = info.shareinfo.name;
	if(info.shareinfo.name == "") (*i)[task_list_columns.share_name] = "Anonymous";
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.protocol]       = info.shareinfo.proto;
	(*i)[task_list_columns.url]            = STRFORMAT("%s:\\\\%s\\%s", info.shareinfo.proto, info.shareinfo.host, info.shareinfo.name);

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	// NOTE: Gtk::ListStore guarantees that iterators are valid as long as the
	// row they reference is valid.
	// See http://library.gnome.org/devel/gtkmm/unstable/classGtk_1_1TreeIter.html#_details
	boost::function<void(const TaskInfo&)> handler =
		dispatcher.wrap(boost::bind(&UFTTWindow::on_signal_task_status, this, i, starttime, _1));
	core->connectSigTaskStatus(info.id, handler);
}

void UFTTWindow::set_backend(UFTTCoreRef _core) {
	dispatcher.invoke(boost::bind(&UFTTWindow::_set_backend, this, _core));
}

void UFTTWindow::_set_backend(UFTTCoreRef _core) {
	if(_core->error_state == 2) { // FIXME: Magic Constant
		Gtk::MessageDialog dialog("Error during initialization", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text(STRFORMAT("There was a problem during the initialization of UFTT's core:\n\n\"%s\"\n\nUFTT can not continue and will now quit.", _core->error_string));
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.run();
//		throw int(1); // thrown integers will quit application with integer as exit code // FIXME: Can't we just quit() nicely?
		on_menu_file_quit();
	}
	if(_core->error_state == 1) {// FIXME: Magic Constant
		Gtk::MessageDialog dialog("Warning during initialization", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
		dialog.set_secondary_text(STRFORMAT("Some warnings were generated during the initialization of UFTT's core:\n\n%s\n\nDo you still want to continue?", _core->error_string));
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.set_default_response(Gtk::RESPONSE_YES);
		if(dialog.run() == Gtk::RESPONSE_NO) {
			on_menu_file_quit();
		}
	}

	this->core = _core;
	core->connectSigAddShare(dispatcher.wrap(boost::bind(&UFTTWindow::on_signal_add_share, this, _1)));
	core->connectSigNewTask(dispatcher.wrap(boost::bind(&UFTTWindow::on_signal_new_task, this, _1)));
}

bool UFTTWindow::on_delete_event(GdkEventAny* event) { // Close button
	on_statusicon_signal_activate();
	return true;
}

void UFTTWindow::on_menu_file_quit() { // FIXME: Disconnect signals etc?
	hide();
	Gtk::Main::quit();
}
