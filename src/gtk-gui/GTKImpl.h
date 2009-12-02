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
	#include <gtkmm/button.h>
	#include <gtkmm/window.h>
	#include <gtkmm/toolbar.h>
	#include <gtkmm/treeview.h>
	#include <gtkmm/textview.h>
	#include <gtkmm/alignment.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/separator.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/toolbutton.h>
	#include <gtkmm/scrolledwindow.h>
	#include <gtkmm/radiobuttongroup.h>
	#include <gtkmm/filechooserbutton.h>
	#include <gtkmm/separatortoolitem.h>

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
						add(share_id);
					}

					Gtk::TreeModelColumn<Glib::ustring> user_name;
					Gtk::TreeModelColumn<Glib::ustring> share_name;
					Gtk::TreeModelColumn<Glib::ustring> host_name;
					Gtk::TreeModelColumn<Glib::ustring> protocol;
					Gtk::TreeModelColumn<Glib::ustring> url;
					Gtk::TreeModelColumn<ShareID>       share_id;
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
			Gtk::VBox                share_list_vbox;
			Gtk::Alignment           share_list_alignment;
			Gtk::Alignment           task_list_alignment;
			Gtk::Alignment           debug_log_alignment;
			Gtk::Alignment           download_destination_path_alignment;
			Gtk::HBox                download_destination_path_hbox;
			Gtk::VBox                download_destination_path_vbox;
			Gtk::Entry               download_destination_path_entry;
			Gtk::Label               download_destination_path_label;
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
			void on_signal_hide();
			void on_share_list_treeview_signal_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);
			void download_selected_shares();
			sigc::connection on_download_destination_path_entry_signal_changed_connection;
			void on_download_destination_path_entry_signal_changed();
			void on_browse_for_download_destination_path_button_signal_current_folder_changed();
			void on_refresh_shares_toolbutton_clicked();
			void on_add_share_file();
			void on_add_share_folder();
			bool refresh_shares();
			void _set_backend(UFTTCoreRef _core);
			void add_share(const ShareInfo& info);
	};

#endif
