#ifndef GMPMPC_H
	#define GMPMPC_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include "AutoScrollingWindow.h"
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
	#include <gtkmm/treeview.h>
	#include <gtkmm/textview.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/scrolledwindow.h>
	#include <gtkmm/radiobuttongroup.h>

	class UFTTWindow : public Gtk::Window {
		public:
			/* Con- & De-structors */
			UFTTWindow(UFTTSettingsRef _settings);

			/* Public functions */
			void set_backend(UFTTCoreRef _core);
		private:
			UFTTCoreRef core;
			UFTTSettingsRef settings;
			/* Variables */
			DispatcherMarshaller dispatcher; // Execute a function in the gui thread
			Glib::RefPtr<Gtk::ListStore> share_list_liststore;

			/* Helpers */
			Glib::RefPtr<Gtk::UIManager>   m_refUIManager;
			Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
			class ShareListColumns : public Gtk::TreeModelColumnRecord {
				public:
					ShareListColumns() {
						add(user_name);
						add(share_name);
						add(host_name);
						add(protocol);
						add(url);
					}

					Gtk::TreeModelColumn<Glib::ustring> user_name;
					Gtk::TreeModelColumn<Glib::ustring> share_name;
					Gtk::TreeModelColumn<Glib::ustring> host_name;
					Gtk::TreeModelColumn<Glib::ustring> protocol;
					Gtk::TreeModelColumn<Glib::ustring> url;
			};
			ShareListColumns share_list_columns;

			/* Widgets */
			Gtk::Menu*               menubar_ptr; // Menu is created dynamically using UIManager
			Gtk::RadioButtonGroup    menu_options_check_updates_frequency_radio_button_group;
			Gtk::VBox                menu_main_paned_vbox;
			Gtk::HPaned              main_paned;
			Gtk::VPaned              share_task_list_vpaned;
			Gtk::Frame               task_list_frame;
			Gtk::Frame               debug_log_frame;
			Gtk::AutoScrollingWindow debug_log_scrolledwindow;
			Gtk::TextView            debug_log_textview;
			Gtk::Frame               share_list_frame;
			Gtk::ScrolledWindow      share_list_scrolledwindow;
			Gtk::TreeView            share_list_treeview;
			Gtk::TreeView            task_list_treeview;

			/* Functions */
			void on_menu_file_quit();
			void on_signal_hide();
			void _set_backend(UFTTCoreRef _core);
			void add_share(const ShareInfo& info);
	};

#endif
