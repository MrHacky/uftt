#include "GTKImpl.h"
#include "OStreamGtkTextBuffer.h"
#include <gtkmm/stock.h>
//#include <gtkmm/actiongroup.h>
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
//#include <gtkmm/recentaction.h>
#include <gtkmm/toggleaction.h>
#include <glib/gthread.h>
#include <ios>

using namespace std;

UFTTWindow::UFTTWindow(UFTTSettingsRef _settings) 
: settings(_settings)
{
	construct_gui();
}

void UFTTWindow::construct_gui() {
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
	share_list_frame.add(share_list_treeview);
	share_task_list_vpaned.add(share_list_frame);
	task_list_frame.set_label("Tasklist:");
	task_list_frame.add(task_list_treeview);
	share_task_list_vpaned.add(task_list_frame);
	main_paned.add(share_task_list_vpaned);
	debug_log_textview.set_buffer(Glib::RefPtr<Gtk::TextBuffer>(new OStreamGtkTextBuffer(std::cout)));
	debug_log_scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	debug_log_scrolledwindow.add(debug_log_textview);
	debug_log_frame.add(debug_log_scrolledwindow);
	debug_log_frame.set_label("Debuglog:");
	main_paned.add(debug_log_frame);
	menu_main_paned_vbox.pack_start(*menubar_ptr, Gtk::PACK_SHRINK);
	menu_main_paned_vbox.add(main_paned);
	add(menu_main_paned_vbox);

	show_all();
	present();
	share_task_list_vpaned.set_position(share_task_list_vpaned.get_height()/2);
	main_paned.set_position(main_paned.get_width()*5/8);
}

void UFTTWindow::on_menu_file_quit() {
	hide();
//	Gtk::Main::quit(); FIXME: Do this when we have a trayicon
}
