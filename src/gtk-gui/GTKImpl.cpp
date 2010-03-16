#include "GTKImpl.h"
#include "OStreamGtkTextBuffer.h"
#include "ShowURI.h"
#include "../Globals.h"
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
#include <gtkmm/accelkey.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/settings.h>
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
  share_list(_settings, *this),
  task_list(_settings),
  uftt_preferences_dialog(_settings)
{
	set_title("UFTT");
	set_default_size(1024, 640);
	property_visible() = false;

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
		add_share_folder_dialog_connection = button->signal_clicked().connect(boost::bind(&UFTTWindow::on_add_share_folder, this));
		add_share_folder_dialog.set_transient_for(*this);
		add_share_folder_dialog.set_modal(true);
#if GTK_CHECK_VERSION(2, 18, 3)
		add_share_folder_dialog.set_create_folders(false);
#endif
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
	m_refActionGroup = Gtk::ActionGroup::create("UFTT");
	Glib::RefPtr<Gtk::Action> action; // Only used when we need to add an accel_path to a menu-item

	/* File menu */
	m_refActionGroup->add(Gtk::Action::create("FileMenu", "_File"));
	m_refActionGroup->add(Gtk::Action::create("FileAddShareFile", Gtk::Stock::FILE, "Share _File..."),
	                      boost::bind(&Gtk::Dialog::present, &add_share_file_dialog));
	m_refActionGroup->add(Gtk::Action::create("FileAddSharefolder", Gtk::Stock::DIRECTORY, "Share F_older..."),
	                      boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog));
	m_refActionGroup->add(Gtk::Action::create("FileDownload", Gtk::Stock::GO_DOWN, "_Download selected"),
	                      boost::bind(&ShareList::download_selected_shares, &share_list));
	m_refActionGroup->add(Gtk::Action::create("FileDownloadTo", Gtk::Stock::GO_DOWN, "Download selected _To..."));/*,
	                      boost::bind(&Gtk::Dialog::present, &add_share_folder_dialog));*/
	m_refActionGroup->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
	                      boost::bind(&UFTTWindow::on_menu_file_quit, this));

	/* Edit Menu */
	m_refActionGroup->add(Gtk::Action::create("EditMenu", "_Edit"));
	action = Gtk::Action::create("EditPreferences", Gtk::Stock::PREFERENCES);
	m_refActionGroup->add(action, boost::bind(&Gtk::Dialog::present, &uftt_preferences_dialog));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/Edit/Preferences");

	/* View Menu */
	m_refActionGroup->add(Gtk::Action::create("ViewMenu", "_View"));
	action = Gtk::Action::create("ViewRefreshShareList",Gtk::Stock::REFRESH, "_Refresh Sharelist");
	m_refActionGroup->add(action, boost::bind(&UFTTWindow::on_refresh_shares_toolbutton_clicked, this));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/RefreshShareList");
	m_refActionGroup->add(Gtk::Action::create("ViewCancelSelectedTasks",Gtk::Stock::MEDIA_STOP, "_Cancel selected tasks"));
	m_refActionGroup->add(Gtk::Action::create("ViewResumeSelectedTasks",Gtk::Stock::MEDIA_PLAY, "_Resume selected tasks"));
	m_refActionGroup->add(Gtk::Action::create("ViewPauseSelectedTasks",Gtk::Stock::MEDIA_PAUSE, "_Pause selected tasks"));
	action = Gtk::Action::create("ViewClearTaskList",Gtk::Stock::CLEAR, "_Clear Completed Tasks");
	m_refActionGroup->add(action, boost::bind(&TaskList::cleanup, &task_list));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/ClearTaskList");

	m_refActionGroup->add(Gtk::ToggleAction::create("ViewDebuglog", "_Debuglog"));
	m_refActionGroup->add(Gtk::ToggleAction::create("ViewManualConnect", "_Manual Connect"));
	action = Gtk::ToggleAction::create("ViewToolbar", "_Toolbar");
	m_refActionGroup->add(action, boost::bind(&TaskList::cleanup, &task_list));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/Toolbar");

	/* Help Menu */
	m_refActionGroup->add(Gtk::Action::create("HelpMenu", "_Help"));
	m_refActionGroup->add(Gtk::Action::create("HelpUserManual", Gtk::Stock::HELP),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/wiki/UFTTHowto"));
	m_refActionGroup->add(Gtk::Action::create("HelpFAQ", "_Frequently Asked Questions"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/wiki/FAQ"));
	m_refActionGroup->add(Gtk::Action::create("HelpBugs", "Report _Bugs/Patches/Requests"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/issues/list"));
	m_refActionGroup->add(Gtk::Action::create("HelpHomePage", Gtk::Stock::HOME, "UFTT _Home Page"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/"));

	m_refActionGroup->add(Gtk::Action::create("HelpAboutUFTT", Gtk::Stock::ABOUT),
	                      boost::bind(&Gtk::AboutDialog::present, &uftt_about_dialog));//, "About _UFTT"));
//	m_refActionGroup->add(Gtk::Action::create("HelpAboutGTK", Gtk::Stock::ABOUT, "About _GTK")); Win32 only?

	/* Various widgets */
	m_refActionGroup->add(Gtk::ToggleAction::create("StatusIconShowUFTT", "_Show UFTT"),
	                      boost::bind(&UFTTWindow::on_statusicon_show_uftt_checkmenuitem_toggled, this));


	m_refUIManager = Gtk::UIManager::create();
	m_refUIManager->insert_action_group(m_refActionGroup);

	add_accel_group(m_refUIManager->get_accel_group());

	/* Layout the actions */
	Glib::ustring ui_info =
		"<ui>"
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
		"    <menu action='EditMenu'>"
		"      <menuitem action='EditPreferences'/>"
		"    </menu>"
		"    <menu action='ViewMenu'>"
		"      <menuitem action='ViewRefreshShareList'/>"
		"      <menuitem action='ViewClearTaskList'/>"
		"      <separator/>"
		"      <menuitem action='ViewDebuglog'/>"
		"      <menuitem action='ViewManualConnect'/>"
		"      <menuitem action='ViewToolbar'/>"
		"    </menu>"
		"    <menu action='HelpMenu'>"
		"      <menuitem action='HelpUserManual'/>"
		"      <menuitem action='HelpFAQ'/>"
		"      <separator/>"
		"      <menuitem action='HelpHomePage'/>"
		"      <menuitem action='HelpBugs'/>"
		"      <separator/>"
		"      <menuitem action='HelpAboutUFTT'/>"
//		"      <menuitem action='HelpAboutGTK'/>" Win32 only
		"    </menu>"
		"  </menubar>"
		"  <popup name='StatusIconPopup'>"
		"    <menuitem action='StatusIconShowUFTT'/>"
		"    <separator/>"
		"    <menuitem action='FileAddShareFile'/>"
		"    <menuitem action='FileAddSharefolder'/>"
		"    <separator/>"
		"    <menuitem action='FileQuit'/>"
		"  </popup>"
		"  <popup name='ShareListSelectionPopup'>"
		"    <menuitem action='FileAddShareFile'/>"
		"    <menuitem action='FileAddSharefolder'/>"
		"    <separator/>"
		"    <menuitem action='FileDownload'/>"
		"    <menuitem action='FileDownloadTo'/>"
		"    <separator/>"
		"    <menuitem action='ViewRefreshShareList'/>"
		"  </popup>"
		"  <popup name='ShareListNoSelectionPopup'>"
		"    <menuitem action='FileAddShareFile'/>"
		"    <menuitem action='FileAddSharefolder'/>"
		"    <separator/>"
		"    <menuitem action='FileDownload'/>"
		"    <menuitem action='FileDownloadTo'/>"
		"    <separator/>"
		"    <menuitem action='ViewRefreshShareList'/>"
		"  </popup>"
		"  <popup name='TaskListSelectionPopup'>"
		"    <menuitem action='ViewPauseSelectedTasks'/>"
		"    <menuitem action='ViewResumeSelectedTasks'/>"
		"    <menuitem action='ViewCancelSelectedTasks'/>"
		"    <separator/>"
		"    <menuitem action='ViewClearTaskList'/>"
		"  </popup>"
		"  <popup name='TaskListNoSelectionPopup'>"
		"    <menuitem action='ViewPauseSelectedTasks'/>"
		"    <menuitem action='ViewResumeSelectedTasks'/>"
		"    <menuitem action='ViewCancelSelectedTasks'/>"
		"    <separator/>"
		"    <menuitem action='ViewClearTaskList'/>"
		"  </popup>"
		"</ui>";

	m_refUIManager->add_ui_from_string(ui_info); //FIXME: This may throw an error if the XML above is not valid
	menubar_ptr = (Gtk::Menu*)m_refUIManager->get_widget("/MenuBar");

	((Gtk::MenuItem*)m_refUIManager->get_widget("/ShareListNoSelectionPopup/FileDownload"))->set_sensitive(false);
	((Gtk::MenuItem*)m_refUIManager->get_widget("/ShareListNoSelectionPopup/FileDownloadTo"))->set_sensitive(false);
	share_list.set_popup_menus(
		(Gtk::Menu*)m_refUIManager->get_widget("/ShareListSelectionPopup"),
		(Gtk::Menu*)m_refUIManager->get_widget("/ShareListNoSelectionPopup")
	);
	share_list_frame.set_label("Sharelist:");
	share_list_frame.add(share_list);
	share_list_alignment.add(share_list_frame);
	share_list_alignment.set_padding(4, 4, 4, 4);

	((Gtk::MenuItem*)m_refUIManager->get_widget("/TaskListNoSelectionPopup/ViewCancelSelectedTasks"))->set_sensitive(false);
	((Gtk::MenuItem*)m_refUIManager->get_widget("/TaskListNoSelectionPopup/ViewResumeSelectedTasks"))->set_sensitive(false);
	((Gtk::MenuItem*)m_refUIManager->get_widget("/TaskListNoSelectionPopup/ViewPauseSelectedTasks"))->set_sensitive(false);
	task_list.set_popup_menus(
		(Gtk::Menu*)m_refUIManager->get_widget("/TaskListSelectionPopup"),
		(Gtk::Menu*)m_refUIManager->get_widget("/TaskListNoSelectionPopup")
	);
	task_list_frame.set_label("Tasklist:");
	task_list_frame.add(task_list);
	task_list_alignment.add(task_list_frame);
	task_list_alignment.set_padding(4, 4, 4, 4);

	share_task_list_vpaned.signal_realize().connect(boost::bind(&UFTTWindow::on_share_task_list_vpaned_realize, this));
	main_paned.signal_realize().connect(boost::bind(&UFTTWindow::on_main_paned_realize, this));
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
	edit_preferences_toolbutton.signal_clicked().connect(boost::bind(&Gtk::Dialog::present, &uftt_preferences_dialog));
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
	menu_main_paned_vbox.show_all();
	add(menu_main_paned_vbox);

	download_shares_toolbutton.set_tooltip_text("Download selected shares");
	edit_preferences_toolbutton.set_tooltip_text("Edit Preferences");
	refresh_shares_toolbutton.set_tooltip_text("Refresh sharelist");
	add_share_file_toolbutton.set_tooltip_text("Share a single file");
	add_share_folder_toolbutton.set_tooltip_text("Share a whole folder");

	vector<string> author_list;
	author_list.push_back("\"CodeAcc\" <http://code.google.com/u/CodeAcc/>");
	author_list.push_back("Daniël Geelen <http://code.google.com/u/daniel.geelen/>");
	author_list.push_back("Simon Sasburg <http://code.google.com/u/simon.sasburg/>");
	uftt_about_dialog.set_authors(author_list);
	uftt_about_dialog.set_comments("A simple no-nonsense tool for transferring files.");
	uftt_about_dialog.set_copyright("\302\251 Copyright 2009 the UFTT Team");
	uftt_about_dialog.set_license("UFTT is free software; you can redistribute it and/or modify\n"
	                              "it under the terms of the GNU General Public License as published by\n"
	                              "the Free Software Foundation; either version 2 of the License, or\n"
	                              "(at your option) any later version.\n"
	                              "\n"
	                              "UFTT is distributed in the hope that it will be useful,\n"
	                              "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	                              "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
	                              "GNU General Public License for more details.\n"
	                              "\n"
	                              "You should have received a copy of the GNU General Public License\n"
	                              "along with UFTT; if not, write to the Free Software Foundation,Inc.,\n"
	                              "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n");
	uftt_about_dialog.set_logo(Glib::RefPtr<Gdk::Pixbuf>()); // empty RefPtr, so uses default icon
	uftt_about_dialog.set_program_name("UFTT");
	uftt_about_dialog.set_version(thebuildstring);
	uftt_about_dialog.set_website("http://code.google.com/p/uftt/");
	uftt_about_dialog.signal_response().connect(boost::bind(&Gtk::AboutDialog::hide, &uftt_about_dialog));
	uftt_about_dialog.set_transient_for(*this);
	uftt_about_dialog.set_modal(true);

	uftt_preferences_dialog.set_transient_for(*this);
	uftt_preferences_dialog.signal_settings_changed.connect(boost::bind(&UFTTWindow::on_apply_settings, this));

	// We might want to save&load the AccelMap, this way a user can redefine
	// shortcuts without us having to provide a seperate configuration window.
	// This does require that the following property be set to true:
	// Gtk::Settings::get_default()->property_gtk_can_change_accels() = true;
	Gtk::AccelMap::change_entry("<UFTT>/MainWindow/MenuBar/Edit/Preferences"     , Gtk::AccelKey( "P").get_key(), Gdk::CONTROL_MASK   , true);
	Gtk::AccelMap::change_entry("<UFTT>/MainWindow/MenuBar/View/RefreshShareList", Gtk::AccelKey("F5").get_key(), Gdk::ModifierType(0), true);
	Gtk::AccelMap::change_entry("<UFTT>/MainWindow/MenuBar/View/ClearTaskList"   , Gtk::AccelKey("F6").get_key(), Gdk::ModifierType(0), true);
	Gtk::AccelMap::change_entry("<UFTT>/MainWindow/MenuBar/View/Toolbar"         , Gtk::AccelKey( "T").get_key(), Gdk::CONTROL_MASK   , true);

	if(settings->loaded) {
		restore_window_size_and_position();
		Gtk::CheckMenuItem* mi = (Gtk::CheckMenuItem*)m_refUIManager->get_widget("/MenuBar/ViewMenu/ViewToolbar");
		mi->signal_toggled().connect(boost::bind(&UFTTWindow::on_view_toolbar_checkmenuitem_signal_toggled, this));
		toolbar.show_all();
		toolbar.set_no_show_all(true);
		mi->set_active(settings->show_toolbar);
		toolbar.property_visible() = settings->show_toolbar;
	}
}

void UFTTWindow::on_main_paned_realize() {
	main_paned.set_position(main_paned.get_width()*5/8);
}

void UFTTWindow::on_share_task_list_vpaned_realize() {
	share_task_list_vpaned.set_position(share_task_list_vpaned.get_height()/2);
}

void UFTTWindow::on_apply_settings() {
	statusicon->set_visible(settings->show_task_tray_icon);

	//TODO: Rebroadcast sharelist (needs backend support)
	//      (re-add every share with new nickname)
	refresh_shares();
}

void UFTTWindow::show_uri(Glib::ustring uri) {
	try {
		Gtk::show_uri(Glib::wrap((GdkScreen*)NULL), uri, GDK_CURRENT_TIME);
	}
	catch(Glib::SpawnError e) {
		Gtk::MessageDialog dialog(*this, "Failed to open URL", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.set_secondary_text(STRFORMAT("UFTT wanted to open the following URI in your default browser,\n"
		                                    "but could not determine what browser you use.\n\n\"%s\"", uri));
		dialog.run();
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

void UFTTWindow::on_view_toolbar_checkmenuitem_signal_toggled() {
	Gtk::CheckMenuItem* mi = (Gtk::CheckMenuItem*)m_refUIManager->get_widget("/MenuBar/ViewMenu/ViewToolbar");
	settings->show_toolbar = mi->get_active();
	toolbar.property_visible() = settings->show_toolbar;
}

void UFTTWindow::restore_window_size_and_position() {
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

void UFTTWindow::on_statusicon_show_uftt_checkmenuitem_toggled() {
	Gtk::CheckMenuItem* m = (Gtk::CheckMenuItem*)m_refUIManager->get_widget("/StatusIconPopup/StatusIconShowUFTT");
	bool b = m->get_active();
	if(b && !is_visible()) {
		show();
	}
	else if(!b && is_visible()) {
		save_window_size_and_position();
		hide();
	}
}

void UFTTWindow::on_statusicon_signal_popup_menu(guint button, guint32 activate_time) {
	((Gtk::CheckMenuItem*)(m_refUIManager->get_widget("/StatusIconPopup/StatusIconShowUFTT")))->set_active(property_visible());
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
			Gtk::MessageDialog dialog(*this, "File does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
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
			Gtk::MessageDialog dialog(*this, "Folder does not exist", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dialog.set_transient_for(add_share_folder_dialog);
			dialog.set_modal(true);
			dialog.set_secondary_text("The folder you have selected to share does not appear to exist.\nPlease verify that the path and foldername are correct and try again.");
			dialog.run();
		}
	}
	add_share_folder_dialog_connection.unblock();
}

void UFTTWindow::refresh_shares() {
	if(core) core->doRefreshShares();
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
			Glib::signal_timeout().connect_once(dispatcher.wrap(boost::bind(&UFTTWindow::refresh_shares, this)), i*20);
		}
	}
}

void UFTTWindow::set_backend(UFTTCore* _core) {
	{
		// FIXME: We probably want to do this *AFTER* checking for errors/warnings
		//         but this causes a nasty exception in SimpleBackend on exit if
		//         we throw().
		this->core = _core;
		task_list.set_backend(core);
		share_list.set_backend(core);
	}

	if(_core->error_state == 2) { // FIXME: Magic Constant
		Gtk::MessageDialog dialog(*this, "Error during initialization", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.set_secondary_text(STRFORMAT("There was a problem during the initialization of UFTT's core:\n\n\"%s\"\n\nUFTT can not continue and will now quit.", _core->error_string));
		dialog.set_transient_for(*this);
		dialog.set_modal(true);
		dialog.set_skip_taskbar_hint(false);
		dialog.run();
		throw int(1); // thrown integers will quit application with integer as exit code
	}
	if(_core->error_state == 1) {// FIXME: Magic Constant
		Gtk::MessageDialog dialog(*this, "Warning during initialization", false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
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

void UFTTWindow::pre_show() {}

void UFTTWindow::post_show() {
	statusicon->set_visible(settings->show_task_tray_icon);
}

bool UFTTWindow::on_delete_event(GdkEventAny* event) { // Close button
	if(settings->show_task_tray_icon && settings->minimize_to_tray_mode == 1) {
		save_window_size_and_position();
		hide();
	}
	else {
		on_menu_file_quit();
	}
	return true;
}


void UFTTWindow::on_menu_file_quit() { // FIXME: Disconnect signals etc?
	{
		// Store settings, FIXME: Check if there are signals we can use to
		// capture these before we need to quit()
		save_window_size_and_position();
	}
	statusicon->set_visible(false);
	hide();
	Gtk::Main::quit();
}
