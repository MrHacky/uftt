#ifndef SHARE_LIST_H
	#define SHARE_LIST_H
	#include "../UFTTCore.h"
	#include "../util/Filesystem.h"
	#include "dispatcher_marshaller.h"
	#include <gtkmm/box.h>
	#include <gtkmm/treeview.h>
	#include <gtkmm/alignment.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/scrolledwindow.h>
	#include <gtkmm/filechooserbutton.h>

	class ShareList : public Gtk::VBox {
		public:
			ShareList(UFTTSettingsRef _settings, Gtk::Window& _parent_window, Glib::RefPtr<Gtk::UIManager> uimanager_ref_);
			void set_backend(UFTTCore* _core);

			/**
			 * Returns the widget that should be used as the target mnemonic
			 * widget by users of class ShareList, as Gtk::VBox is not suitable
			 * for mnemonic activation.
			 *
			 * The widget returned by this function is intended to be passed
			 * to the set_mnemomic_widget() functions of widgets intending to
			 * activate the ShareList.
			 *
			 * @return a GTK::Widget suitable for passing to set_mnemomic_widget().
			 */
			Gtk::Widget* get_mnemonic_widget();

		private:
			UFTTCore* core;
			UFTTSettingsRef settings;
			DispatcherMarshaller dispatcher; // Execute a function in the gui thread
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
			ShareListColumns              share_list_columns;
			Glib::RefPtr<Gtk::ListStore>  share_list_liststore;
			Gtk::TreeView                 share_list_treeview;
			Gtk::ScrolledWindow           share_list_scrolledwindow;
			Gtk::Alignment                download_destination_path_alignment;
			Gtk::HBox                     download_destination_path_hbox;
			Gtk::VBox                     download_destination_path_vbox;
			Gtk::Entry                    download_destination_path_entry;
			Gtk::Label                    download_destination_path_label;
			Gtk::FileChooserDialog        pick_download_destination_dialog; // Override download destination for single download
			sigc::connection              add_share_file_dialog_connection;
			Gtk::FileChooserDialog        add_share_file_dialog;
			sigc::connection              add_share_folder_dialog_connection;
			Gtk::FileChooserDialog        add_share_folder_dialog;
			Gtk::FileChooserButton        browse_for_download_destination_path_button;
			Glib::RefPtr<Gtk::UIManager>  uimanager_ref;

			// Functions / callbacks
			sigc::connection on_download_destination_path_entry_signal_changed_connection;
			void on_download_destination_path_entry_signal_changed();
			void on_browse_for_download_destination_path_button_signal_current_folder_changed();
			void on_share_list_treeview_signal_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);
			void on_share_list_treeview_selection_signal_changed();
			void on_signal_add_share(const ShareInfo& info);
			void on_add_share_file_dialog_button_clicked();
			void on_add_share_folder_dialog_button_clicked();
			void on_pick_download_destination_dialog_button_clicked();
			void on_refresh_shares();
			bool on_share_list_treeview_signal_button_press_event(GdkEventButton* event);
			bool on_share_list_treeview_signal_key_press_event(GdkEventKey* event);
			void download_selected_shares();
			void download_selected_shares(ext::filesystem::path destination);
			void remove_selected_shares();
	};

#endif // SHARE_LIST_H
