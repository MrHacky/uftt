#ifndef TASK_LIST_H
	#define TASK_LIST_H
	#include "../UFTTCore.h"
	#include "dispatcher_marshaller.h"
	#include "Notification.h"
	#include <gtkmm/stock.h>
	#include <gtkmm/treeview.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/scrolledwindow.h>
	#include <list>
	#include <boost/date_time/posix_time/posix_time.hpp>
	#include <boost/signals.hpp>

	class TaskList : public Gtk::ScrolledWindow {
		public:
			TaskList(UFTTSettingsRef _settings, Glib::RefPtr<Gtk::UIManager> uimanager_ref_);
			void set_backend(UFTTCore* _core);
			// nr downloads, downloadspeed, nr uploads, upload speed
			boost::signal<void(uint32, uint32, uint32, uint32)> signal_status;
		private:
			UFTTCore* core;
			UFTTSettingsRef settings;
			DispatcherMarshaller dispatcher; // Execute a function in the gui thread
			class TaskListColumns : public Gtk::TreeModelColumnRecord {
				public:
					TaskListColumns() {
						add(status);
						add(status_string);
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

					Gtk::TreeModelColumn<TaskStatus>    status;
					Gtk::TreeModelColumn<Glib::ustring> status_string;
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
			TaskListColumns               task_list_columns;
			Glib::RefPtr<Gtk::ListStore>  task_list_liststore;
			Gtk::TreeView                 task_list_treeview;
			Glib::RefPtr<Gtk::UIManager>  uimanager_ref;

			// Notification of task completions
			std::list<TaskInfo> completed_tasks; // A list of tasks completed since the last check
			void check_completed_tasks();        // A timeout handler to merge tasks which complete within a certain time window of eachother
			boost::shared_ptr<Gtk::Notification> notification;
			boost::posix_time::ptime  last_notification;
			boost::posix_time::ptime  last_completion;
			Glib::RefPtr<Gdk::Pixbuf> ufft_icon;

			// Cumulative task status
			boost::posix_time::ptime last_status_update;

			/* Functions */
			void on_signal_task_status(const boost::shared_ptr<Gtk::TreeModel::RowReference> rowref, const TaskInfo& info);
			void on_signal_new_task(const TaskInfo& info);
			bool on_task_list_treeview_signal_button_press_event(GdkEventButton* event);
			void on_task_list_treeview_selection_signal_changed();
			void on_task_list_treeview_signal_row_inserted_deleted();
			void cleanup();
			void execute_selected_tasks();
			void open_folder_selected_tasks();
	};
#endif // TASK_LIST_H
