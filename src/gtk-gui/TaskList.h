#ifndef TASK_LIST_H
	#define TASK_LIST_H
	#include "../UFTTCore.h"
	#include "dispatcher_marshaller.h"
	#include <gtkmm/treeview.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/scrolledwindow.h>

	class TaskList : public Gtk::ScrolledWindow {
		public:
			TaskList();
			void on_signal_task_status(const Gtk::TreeModel::iterator i, const TaskInfo& info);
			void on_signal_new_task(const TaskInfo& info);
			void set_backend(UFTTCore* _core);
			void set_popup_menu(Gtk::Menu* _popup_menu);
		private:
			UFTTCore* core;
			DispatcherMarshaller dispatcher; // Execute a function in the gui thread
			class TaskListColumns : public Gtk::TreeModelColumnRecord {
				public:
					TaskListColumns() {
						add(status);
						add(time_elapsed);
						add(time_remaining);
						add(transferred);
						add(total_size);
						add(speed);
						add(queue);
						add(task_info);
						add(user_name);
						add(share_name);
						add(host_name);
						add(protocol);
						add(url);
					}

					Gtk::TreeModelColumn<Glib::ustring> status;
					Gtk::TreeModelColumn<Glib::ustring> time_elapsed;
					Gtk::TreeModelColumn<Glib::ustring> time_remaining;
					Gtk::TreeModelColumn<Glib::ustring> transferred;
					Gtk::TreeModelColumn<Glib::ustring> total_size;
					Gtk::TreeModelColumn<Glib::ustring> speed;
					Gtk::TreeModelColumn<uint32>        queue;
					Gtk::TreeModelColumn<TaskInfo>      task_info;
					// TaskInfo contains a ShareInfo, so list that here too
					// NOTE: We're intentionally not inheriting from TaskListColumns
					Gtk::TreeModelColumn<Glib::ustring> user_name;
					Gtk::TreeModelColumn<Glib::ustring> share_name;
					Gtk::TreeModelColumn<Glib::ustring> host_name;
					Gtk::TreeModelColumn<Glib::ustring> protocol;
					Gtk::TreeModelColumn<Glib::ustring> url;
			};
			TaskListColumns              task_list_columns;
			Glib::RefPtr<Gtk::ListStore> task_list_liststore;
			Gtk::TreeView                task_list_treeview;
			Gtk::Menu*                    popup_menu;
			/* Functions */
			bool on_task_list_treeview_signal_button_press_event(GdkEventButton* event);
	};
#endif // TASK_LIST_H
