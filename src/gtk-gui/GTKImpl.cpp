#include "GTKImpl.h"
#include "OStreamGtkTextBuffer.h"
#include "ShowURI.h"
#include "../BuildString.h"
#include "../util/StrFormat.h"
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

#define UFTT_TITLE_BASE   "gUFTT"
#define UFTT_TOOLTIP_BASE "The Ultimate File Transfer Tool"

UFTTWindow::UFTTWindow(UFTTSettingsRef _settings)
: settings(_settings),
  uimanager_ref(Gtk::UIManager::create()),
  share_list(_settings, *this, uimanager_ref),
  task_list(_settings, uimanager_ref),
  uftt_preferences_dialog(_settings)
{
	set_title(UFTT_TITLE_BASE);
	set_default_size(1024, 640);
	property_visible() = false;

	statusicon_pixbuf = get_best_uftt_icon_for_size(256, 256);
	statusicon = Gtk::StatusIcon::create(statusicon_pixbuf);
	set_default_icon(statusicon_pixbuf);
	statusicon->set_tooltip(UFTT_TITLE_BASE"\n"UFTT_TOOLTIP_BASE);
	statusicon->set_visible(false);
	{
	sigc::slot<bool, int> slot = sigc::mem_fun(*this, &UFTTWindow::on_statusicon_signal_size_changed);
	statusicon->signal_size_changed().connect(slot);
	}
	statusicon->signal_popup_menu().connect(boost::bind(&UFTTWindow::on_statusicon_signal_popup_menu, this, _1, _2));
	statusicon->signal_activate().connect(boost::bind(&UFTTWindow::on_statusicon_signal_activate, this));

	task_list.signal_status.connect(
		dispatcher.wrap(
			boost::bind(&UFTTWindow::on_signal_task_status, this, _1, _2, _3, _4)
		)
	);
	task_list.signal_task_completed.connect(
		dispatcher.wrap(
			boost::bind(&UFTTWindow::on_signal_task_completed, this, _1)
		)
	);
	{
	sigc::slot<bool, GdkEventFocus*> slot = sigc::mem_fun(*this, &UFTTWindow::on_signal_focus_in_event);
	signal_focus_in_event().connect(slot);
	}

	std::vector<Glib::RefPtr<Gdk::Pixbuf> > icon_list;
	icon_list.push_back(get_best_uftt_icon_for_size(16, 16));
//	icon_list.push_back(get_best_uftt_icon_for_size(22, 22));
	icon_list.push_back(get_best_uftt_icon_for_size(32, 32));
	icon_list.push_back(get_best_uftt_icon_for_size(48, 48));
	set_default_icon_list(icon_list);


	/* Create actions */
	Glib::RefPtr<Gtk::ActionGroup> actiongroup_ref(Gtk::ActionGroup::create("UFTT"));
	Glib::RefPtr<Gtk::Action> action; // Only used when we need to add an accel_path to a menu-item

	/* File menu */
	actiongroup_ref->add(Gtk::Action::create("FileMenu", "_File"));
	actiongroup_ref->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
	                      boost::bind(&UFTTWindow::on_menu_file_quit, this));

	/* Edit Menu */
	actiongroup_ref->add(Gtk::Action::create("EditMenu", "_Edit"));
	action = Gtk::Action::create("EditPreferences", Gtk::Stock::PREFERENCES);
	actiongroup_ref->add(action, boost::bind(&Gtk::Dialog::present, &uftt_preferences_dialog));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/Edit/Preferences");

	/* View Menu */
	actiongroup_ref->add(Gtk::Action::create("ViewMenu", "_View"));
	actiongroup_ref->add(Gtk::ToggleAction::create("ViewDebuglog", "_Debuglog"));
	actiongroup_ref->add(Gtk::ToggleAction::create("ViewManualConnect", "_Manual Connect"));
	action = Gtk::ToggleAction::create("ViewToolbar", "_Toolbar");
	actiongroup_ref->add(action, boost::bind(&UFTTWindow::on_view_toolbar_checkmenuitem_signal_toggled, this));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/Toolbar");

	/* Help Menu */
	actiongroup_ref->add(Gtk::Action::create("HelpMenu", "_Help"));
	actiongroup_ref->add(Gtk::Action::create("HelpUserManual", Gtk::Stock::HELP),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/wiki/UFTTHowto"));
	actiongroup_ref->add(Gtk::Action::create("HelpFAQ", "_Frequently Asked Questions"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/wiki/FAQ"));
	actiongroup_ref->add(Gtk::Action::create("HelpBugs", "Report _Bugs/Patches/Requests"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/issues/list"));
	actiongroup_ref->add(Gtk::Action::create("HelpHomePage", Gtk::Stock::HOME, "UFTT _Home Page"),
	                      boost::bind(&UFTTWindow::show_uri, this, "http://code.google.com/p/uftt/"));

	actiongroup_ref->add(Gtk::Action::create("HelpAboutUFTT", Gtk::Stock::ABOUT),
	                      boost::bind(&Gtk::AboutDialog::present, &uftt_about_dialog));//, "About _UFTT"));
//	actiongroup_ref->add(Gtk::Action::create("HelpAboutGTK", Gtk::Stock::ABOUT, "About _GTK")); Win32 only?

	/* Various widgets */
	actiongroup_ref->add(Gtk::ToggleAction::create("StatusIconShowUFTT", "_Show UFTT"),
	                      boost::bind(&UFTTWindow::on_statusicon_show_uftt_checkmenuitem_toggled, this));

	uimanager_ref->insert_action_group(actiongroup_ref);
	add_accel_group(uimanager_ref->get_accel_group());

	/* Layout the actions */
	Glib::ustring ui_info =
		"<ui>"
		"  <menubar name='MenuBar'>"
		"    <menu action='FileMenu'>"
		"      <menuitem action='FileAddShareFile'/>"
		"      <menuitem action='FileAddShareFolder'/>"
		"      <separator/>"
		"      <menuitem action='FileQuit'/>"
		"    </menu>"
		"    <menu action='EditMenu'>"
		"      <menuitem action='EditPreferences'/>"
		"    </menu>"
		"    <menu action='ShareMenu'>"
		"      <menuitem action='ShareDownload'/>"
		"      <menuitem action='ShareDownloadTo'/>"
		"      <separator/>"
		"      <menuitem action='ShareRemoveShare'/>"
		"    </menu>"
		"    <menu action='TaskMenu'>"
		"      <menuitem action='TaskExecute'/>"
		"      <menuitem action='TaskOpenContainingFolder'/>"
		"      <separator/>"
		"      <menuitem action='TaskPause'/>"
		"      <menuitem action='TaskResume'/>"
		"      <menuitem action='TaskCancel'/>"
		"      <separator/>"
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
		"  <toolbar name='Toolbar'>"
		"    <toolitem action='FileAddShareFile'/>"
		"    <toolitem action='FileAddShareFolder'/>"
		"    <separator/>"
		"    <toolitem action='ViewRefreshShareList'/>"
		"    <toolitem action='ShareDownload'/>"
		"    <separator/>"
		"    <toolitem action='EditPreferences'/>"
		"  </toolbar>"
		"  <popup name='StatusIconPopup'>"
		"    <menuitem action='StatusIconShowUFTT'/>"
		"    <separator/>"
		"    <menuitem action='FileAddShareFile'/>"
		"    <menuitem action='FileAddShareFolder'/>"
		"    <separator/>"
		"    <menuitem action='FileQuit'/>"
		"  </popup>"
		"  <popup name='ShareListPopup'>"
		"    <menuitem action='FileAddShareFile'/>"
		"    <menuitem action='FileAddShareFolder'/>"
		"    <separator/>"
		"    <menuitem action='ShareDownload'/>"
		"    <menuitem action='ShareDownloadTo'/>"
		"    <separator/>"
		"    <menuitem action='ShareRemoveShare'/>"
		"    <separator/>"
		"    <menuitem action='ViewRefreshShareList'/>"
		"  </popup>"
		"  <popup name='TaskListPopup'>"
		"    <menuitem action='TaskExecute'/>"
		"    <menuitem action='TaskOpenContainingFolder'/>"
		"    <separator/>"
		"    <menuitem action='TaskPause'/>"
		"    <menuitem action='TaskResume'/>"
		"    <menuitem action='TaskCancel'/>"
		"    <separator/>"
		"    <menuitem action='ViewClearTaskList'/>"
		"  </popup>"
		"</ui>";
	uimanager_ref->add_ui_from_string(ui_info);

	share_list_frame.set_label("Shares:");
	share_list_frame.add(share_list);
	share_list_alignment.add(share_list_frame);
	share_list_alignment.set_padding(4, 4, 4, 4);

/*
	Pango::FontDescription bold_font_description;
	bold_font_description.set_weight(Pango::WEIGHT_BOLD);
	Pango::AttrFontDesc afd(Pango::Attribute::create_attr_font_desc(bold_font_description));
	afd.set_desc(bold_font_description);
	Pango::AttrList attr_list;
	attr_list.insert(afd);

	 FIXME: This seems really hacky, is bolding menu/popup entries not
	        according to the HIG? It does look 'unnatural'...
	((Gtk::Label*)(((Gtk::MenuItem*)uimanager_ref->get_widget("/TaskListSelectionPopup/PopupTasklistExecute"))->get_child()))->set_attributes(attr_list);
*/

	task_list_frame.set_label("Tasks:");
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
	debug_log_frame.set_label("Log:");
	debug_log_alignment.add(debug_log_frame);
	debug_log_alignment.set_padding(4, 4, 4, 4);
	main_paned.add(debug_log_alignment);
	menu_main_paned_vbox.pack_start(*(Gtk::Menu*)uimanager_ref->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
	menu_main_paned_vbox.pack_start(*uimanager_ref->get_widget("/Toolbar"), Gtk::PACK_SHRINK);
	menu_main_paned_vbox.add(main_paned);
	menu_main_paned_vbox.show_all();
	add(menu_main_paned_vbox);

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
	uftt_about_dialog.set_version(get_build_string());
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

	restore_window_size_and_position();
	uimanager_ref->get_widget("/Toolbar")->show_all();
	uimanager_ref->get_widget("/Toolbar")->set_no_show_all(true);
	((Gtk::CheckMenuItem*)uimanager_ref->get_widget("/MenuBar/ViewMenu/ViewToolbar"))->set_active(settings->show_toolbar);
	uimanager_ref->get_widget("/Toolbar")->property_visible() = settings->show_toolbar;
}

void UFTTWindow::on_main_paned_realize() {
	main_paned.set_position(main_paned.get_width()*5/8);
}

bool UFTTWindow::on_signal_focus_in_event(GdkEventFocus* event) {
	statusicon->set_blinking(false);
	return true;
}

void UFTTWindow::on_signal_task_status(uint32 nr_downloads, uint32 download_speed, uint32 nr_uploads, uint32 upload_speed) {
	if(nr_downloads + nr_uploads > 0 && settings->show_speeds_in_titlebar) {
		set_title(
			STRFORMAT(
				"D:%s U:%s - "UFTT_TITLE_BASE,
				StrFormat::bytes(download_speed, false, true),
				StrFormat::bytes(upload_speed, false, true)
			)
		);
	}
	else {
		set_title(UFTT_TITLE_BASE);
	}

	if(nr_downloads + nr_uploads > 0 && settings->show_task_tray_icon && settings->show_speeds_in_statusicon_tooltip) {
		statusicon->set_tooltip(
			STRFORMAT(
				UFTT_TITLE_BASE"\n%i downloading, %i uploading\n%s down, %s up",
				nr_downloads,
				nr_uploads,
				StrFormat::bytes(download_speed, true),
				StrFormat::bytes(upload_speed, true)
			)
		);
	}
	else {
		statusicon->set_tooltip(UFTT_TITLE_BASE"\n"UFTT_TOOLTIP_BASE);
	}
}

void UFTTWindow::on_signal_task_completed(bool user_acknowledged_completion) {
	statusicon->set_blinking(
		   !property_has_toplevel_focus()
		&& !user_acknowledged_completion
		&& settings->blink_statusicon_on_completion
	);
}

void UFTTWindow::on_share_task_list_vpaned_realize() {
	share_task_list_vpaned.set_position(share_task_list_vpaned.get_height()/2);
}

void UFTTWindow::on_apply_settings() {
	statusicon->set_visible(settings->show_task_tray_icon);
	if(!settings->show_speeds_in_titlebar) {
		set_title(UFTT_TITLE_BASE);
	}
	if(!settings->show_task_tray_icon || !settings->show_speeds_in_statusicon_tooltip) {
		statusicon->set_tooltip(UFTT_TITLE_BASE"\n"UFTT_TOOLTIP_BASE);
	}

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
	Gtk::CheckMenuItem* mi = (Gtk::CheckMenuItem*)uimanager_ref->get_widget("/MenuBar/ViewMenu/ViewToolbar");
	settings->show_toolbar = mi->get_active();
	uimanager_ref->get_widget("/Toolbar")->property_visible() = settings->show_toolbar;
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
	Gtk::CheckMenuItem* m = (Gtk::CheckMenuItem*)uimanager_ref->get_widget("/StatusIconPopup/StatusIconShowUFTT");
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
	((Gtk::CheckMenuItem*)(uimanager_ref->get_widget("/StatusIconPopup/StatusIconShowUFTT")))->set_active(property_visible());
	Gtk::Menu* m = (Gtk::Menu*)(uimanager_ref->get_widget("/StatusIconPopup"));
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

void UFTTWindow::refresh_shares() {
	if(core) core->doRefreshShares();
}

void UFTTWindow::handle_uftt_core_gui_command(UFTTCore::GuiCommand cmd) {
	switch(cmd) {
		case UFTTCore::GUI_CMD_SHOW: {
			restore_window_size_and_position();
			present();
		}; break;
		default: {
			std::cout << "WARNING: Unhandled UFTTCore::GuiCommand" << cmd << std::endl;
		}; break;
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
		core->connectSigGuiCommand(dispatcher.wrap(boost::bind(&UFTTWindow::handle_uftt_core_gui_command, this, _1)));
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
	if(settings->show_task_tray_icon && settings->close_to_tray) {
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
