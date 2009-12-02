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
#include <gtkmm/separatortoolitem.h>

using namespace std;

UFTTWindow::UFTTWindow(UFTTSettingsRef _settings)
: settings(_settings),
  add_share_file_dialog(*this, "Select a file", Gtk::FILE_CHOOSER_ACTION_OPEN),
  add_share_folder_dialog(*this, "Select a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER),
  refresh_shares_toolbutton(Gtk::Stock::REFRESH),
  download_shares_toolbutton(Gtk::Stock::GO_DOWN),
  edit_preferences_toolbutton(Gtk::Stock::PREFERENCES),
  add_share_file_toolbutton(Gtk::Stock::FILE),
  add_share_folder_toolbutton(Gtk::Stock::DIRECTORY),
  share_list(_settings, *this)
{
	set_title("UFTT");
	set_default_size(1024, 640);
	set_position(Gtk::WIN_POS_CENTER);
	set_visible(false);

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
	statusicon->set_visible(false);
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
	m_refActionGroup->add(Gtk::Action::create("FileDownload", Gtk::Stock::GO_DOWN, "Download"),
	                      boost::bind(&ShareList::download_selected_shares, &share_list));
	m_refActionGroup->add(Gtk::Action::create("FileDownloadTo", Gtk::Stock::GO_DOWN, "Download to..."));/*,
	                      boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog));*/
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
	                        "      <menuitem action='FileDownload'/>"
	                        "      <menuitem action='FileDownloadTo'/>"
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

	share_list_frame.set_label("Sharelist:");
	share_list_frame.add(share_list);
	share_list_alignment.add(share_list_frame);
	share_list_alignment.set_padding(4, 4, 4, 4);

	task_list_frame.set_label("Tasklist:");
	task_list_frame.add(task_list);
	task_list_alignment.add(task_list_frame);
	task_list_alignment.set_padding(4, 4, 4, 4);
	
	share_task_list_vpaned.add(share_list_alignment);
	share_task_list_vpaned.add(task_list_alignment);
	main_paned.add(share_task_list_vpaned);
	
	debug_log_textview.set_buffer(Glib::RefPtr<Gtk::TextBuffer>(new OStreamGtkTextBuffer(std::cout)));
	debug_log_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	debug_log_scrolledwindow.add(debug_log_textview);
	debug_log_frame.add(debug_log_scrolledwindow);
	debug_log_frame.set_label("Debuglog:");
	debug_log_alignment.add(debug_log_frame);
	debug_log_alignment.set_padding(4, 4, 4, 4);
	main_paned.add(debug_log_alignment);
	menu_main_paned_vbox.pack_start(*menubar_ptr, Gtk::PACK_SHRINK);

	Gtk::SeparatorToolItem* separator;
	refresh_shares_toolbutton.set_label("Refresh");
	download_shares_toolbutton.set_label("Download");
	edit_preferences_toolbutton.set_label("Preferences");
	add_share_file_toolbutton.set_label("Share file");
	add_share_folder_toolbutton.set_label("Share folder");
	download_shares_toolbutton.signal_clicked().connect(boost::bind(&ShareList::download_selected_shares, &share_list));
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
	download_shares_toolbutton.set_tooltip_text("Download selected shares");
	edit_preferences_toolbutton.set_tooltip_text("Edit Preferences");
	refresh_shares_toolbutton.set_tooltip_text("Refresh sharelist");
	add_share_file_toolbutton.set_tooltip_text("Share a single file");
	add_share_folder_toolbutton.set_tooltip_text("Share a whole folder");

	if(settings->loaded) {
		restore_window_size_and_position();
	}
}

void UFTTWindow::save_window_size_and_position() {
	int x,y;
	get_position(x, y); // NOTE: Need to get_position *before* hide()
	settings->posx = x;
	settings->posy = y;
	int w,h;
	get_size(w, h);
	settings->sizex = w;
	settings->sizey = h;
}

void UFTTWindow::restore_window_size_and_position() {
	set_position(Gtk::WIN_POS_NONE);
	move(settings->posx, settings->posy);
	resize(settings->sizex, settings->sizey);
}

void UFTTWindow::on_statusicon_signal_activate() {
	if( is_visible() ) {
		if( property_is_active() ) {
			save_window_size_and_position();
			hide();
		}
		else {
			restore_window_size_and_position();
			present();
		}
	}
	else {
		restore_window_size_and_position();
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
			if(boost::filesystem::is_directory(path)) {
				add_share_file_dialog.set_current_folder(filename);
			}
			else {
				add_share_file_dialog.hide();
				core->addLocalShare(path.leaf(), path);
			}
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
		share_list.clear();
	}
	if((mask & Gdk::CONTROL_MASK) != Gdk::CONTROL_MASK) {
		for(int i=0; i<8; ++i) {
			sigc::slot<bool> slot = sigc::mem_fun(*this, &UFTTWindow::refresh_shares);
			Glib::signal_timeout().connect(slot, i*20);
		}
	}
}

void UFTTWindow::set_backend(UFTTCoreRef _core) {
	{
		// FIXME: We probably want to do this *AFTER* checking for errors/warnings
		//         but this causes a nasty exception in SimpleBackend on exit if
		//         we throw().
		this->core = _core;
		task_list.set_backend(core);
		share_list.set_backend(core);
	}

	if(_core->error_state == 2) { // FIXME: Magic Constant
		Gtk::MessageDialog dialog("Error during initialization", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text(STRFORMAT("There was a problem during the initialization of UFTT's core:\n\n\"%s\"\n\nUFTT can not continue and will now quit.", _core->error_string));
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.set_skip_taskbar_hint(false);
		dialog.run();
		throw int(1); // thrown integers will quit application with integer as exit code
	}
	if(_core->error_state == 1) {// FIXME: Magic Constant
		Gtk::MessageDialog dialog("Warning during initialization", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
		dialog.set_secondary_text(STRFORMAT("Some warnings were generated during the initialization of UFTT's core:\n\n%s\n\nDo you still want to continue?", _core->error_string));
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.set_skip_taskbar_hint(false);
		dialog.set_default_response(Gtk::RESPONSE_YES);
		if(dialog.run() == Gtk::RESPONSE_NO) {
			throw int(0); // thrown integers will quit application with integer as exit code
		}
	}
}

void UFTTWindow::pre_show() {
	statusicon->set_visible(true);
}

void UFTTWindow::post_show() {
	// FIXME: Because the window is already visible we get an ugly presentation
	//         to the user because the user can now see the layouting process
	//         for the paneds in action.
	share_task_list_vpaned.set_position(share_task_list_vpaned.get_height()/2);
	main_paned.set_position(main_paned.get_width()*5/8);
}

bool UFTTWindow::on_delete_event(GdkEventAny* event) { // Close button
	// TODO:
	// if self.config["close_to_tray"] and self.config["enable_system_tray"]:
	save_window_size_and_position();
	hide();
	// else:
	// on_menu_file_quit();
	return true;
}


void UFTTWindow::on_menu_file_quit() { // FIXME: Disconnect signals etc?
	{
		// Store settings, FIXME: Check if there are signals we can use to
		// capture these before we need to quit()
		save_window_size_and_position();
	}
	hide();
	Gtk::Main::quit();
}
