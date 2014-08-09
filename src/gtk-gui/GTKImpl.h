#ifndef GMPMPC_H
	#define GMPMPC_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include "AutoScrollingWindow.h"
	#include "ShareList.h"
	#include "TaskList.h"
	#include "PreferencesDialog.h"
	#include "dispatcher_marshaller.h"
	#include <iostream>
	#include <boost/signals2/signal.hpp>
	#include <boost/shared_ptr.hpp>
	#include <gtkmm/box.h>
	#include <gtkmm/main.h>
	#include <gtkmm/menu.h>
	#include <gtkmm/paned.h>
	#include <gtkmm/frame.h>
	#include <gtkmm/window.h>
	#include <gtkmm/textview.h>
	#include <gtkmm/alignment.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/statusicon.h>
	#include <gtkmm/toolbutton.h>
	#include <gtkmm/aboutdialog.h>
	#include <gtkmm/radiobuttongroup.h>

	class UFTTWindow : public Gtk::Window {
		public:
			/* Con- & De-structors */
			UFTTWindow(UFTTSettingsRef _settings);

			/* Public functions */
			void set_backend(UFTTCore* _core);
			void pre_show();
			void post_show();
		private:
			/* Variables */
			UFTTCore*                     core;
			UFTTSettingsRef               settings;
			DispatcherMarshaller          dispatcher; // Execute a function in the gui thread
			Glib::RefPtr<Gdk::Pixbuf>     statusicon_pixbuf;
			Glib::RefPtr<Gtk::StatusIcon> statusicon;

			/* Helpers */
			Glib::RefPtr<Gtk::UIManager>   uimanager_ref;  // NOTE: UIManager is referenced by TaskList and ShareList

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
			Gtk::AboutDialog         uftt_about_dialog;
			UFTTPreferencesDialog    uftt_preferences_dialog;

			/* Functions */
			void on_menu_file_quit();
			bool on_delete_event(GdkEventAny* event);
			void on_refresh_shares_toolbutton_clicked();
			void refresh_shares();
			void on_apply_settings();
			bool on_statusicon_signal_size_changed(int xy);
			Glib::RefPtr<Gdk::Pixbuf> get_best_uftt_icon_for_size(int x, int y);
			void on_statusicon_signal_popup_menu(guint button, guint32 activate_time);
			void on_statusicon_signal_activate();
			void on_statusicon_show_uftt_checkmenuitem_toggled();
			void on_main_paned_realize();
			void on_share_task_list_vpaned_realize();
			void save_window_size_and_position();
			void restore_window_size_and_position();
			void on_view_toolbar_checkmenuitem_signal_toggled();
			void show_uri(Glib::ustring uri);
			void handle_uftt_core_gui_command(UFTTCore::GuiCommand cmd);

			void on_signal_task_status(uint32 nr_downloads, uint32 download_speed, uint32 nr_uploads, uint32 upload_speed);
			#ifdef USE_GTK24_API
			void on_signal_task_completed(bool user_acknowledged_completion);
			bool on_signal_focus_in_event(GdkEventFocus* event);
			#endif
	};

#endif
