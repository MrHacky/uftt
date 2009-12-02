#ifndef GMPMPC_H
	#define GMPMPC_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include "AutoScrollingWindow.h"
	#include "ShareList.h"
	#include "TaskList.h"
	#include "dispatcher_marshaller.h"
	#include <iostream>
	#include <boost/signal.hpp>
	#include <boost/shared_ptr.hpp>
	#include <gtkmm/box.h>
	#include <gtkmm/main.h>
	#include <gtkmm/menu.h>
	#include <gtkmm/paned.h>
	#include <gtkmm/frame.h>
	#include <gtkmm/window.h>
	#include <gtkmm/toolbar.h>
	#include <gtkmm/textview.h>
	#include <gtkmm/alignment.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/statusicon.h>
	#include <gtkmm/toolbutton.h>
	#include <gtkmm/radiobuttongroup.h>

	class UFTTWindow : public Gtk::Window {
		public:
			/* Con- & De-structors */
			UFTTWindow(UFTTSettingsRef _settings);

			/* Public functions */
			void set_backend(UFTTCoreRef _core);
			void pre_show();
			void post_show();
		private:
			/* Variables */
			UFTTCoreRef                   core;
			UFTTSettingsRef               settings;
			DispatcherMarshaller          dispatcher; // Execute a function in the gui thread
			Glib::RefPtr<Gdk::Pixbuf>     statusicon_pixbuf;
			Glib::RefPtr<Gtk::StatusIcon> statusicon;

			/* Helpers */
			Glib::RefPtr<Gtk::UIManager>   m_refUIManager;
			Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;

			/* Widgets */
			Gtk::Menu*               menubar_ptr; // Menu is created dynamically using UIManager
			Gtk::VBox                menu_main_paned_vbox;
			Gtk::HPaned              main_paned;
			Gtk::VPaned              share_task_list_vpaned;
			Gtk::Frame               debug_log_frame;
			Gtk::AutoScrollingWindow debug_log_scrolledwindow;
			Gtk::TextView            debug_log_textview;
			Gtk::Frame               share_list_frame;
			Gtk::Frame               task_list_frame;
			ShareList                share_list;
			TaskList                 task_list;
			Gtk::Alignment           share_list_alignment;
			Gtk::Alignment           task_list_alignment;
			Gtk::Alignment           debug_log_alignment;
			sigc::connection         add_share_file_dialog_connection;
			Gtk::FileChooserDialog   add_share_file_dialog;
			sigc::connection         add_share_folder_dialog_connection;
			Gtk::FileChooserDialog   add_share_folder_dialog;
			Gtk::FileChooserButton   browse_for_download_destination_path_button;
			Gtk::Toolbar             toolbar;
			Gtk::Image               refresh_shares_image;
			Gtk::ToolButton          download_shares_toolbutton;
			Gtk::ToolButton          refresh_shares_toolbutton;
			Gtk::ToolButton          edit_preferences_toolbutton;
			Gtk::ToolButton          add_share_file_toolbutton;
			Gtk::ToolButton          add_share_folder_toolbutton;

			/* Functions */
			void on_menu_file_quit();
			bool on_delete_event(GdkEventAny* event);
			void on_refresh_shares_toolbutton_clicked();
			void on_add_share_file();
			void on_add_share_folder();
			bool refresh_shares();
			bool on_statusicon_signal_size_changed(int xy);
			Glib::RefPtr<Gdk::Pixbuf> get_best_uftt_icon_for_size(int x, int y);
			void on_statusicon_signal_popup_menu(guint button, guint32 activate_time);
			void on_statusicon_signal_activate();
			void save_window_size_and_position();
			void restore_window_size_and_position();
			void on_view_toolbar_checkmenuitem_signal_toggled();
	};

#endif
