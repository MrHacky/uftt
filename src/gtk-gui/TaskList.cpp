#include "TaskList.h"
#include "ShowURI.h"
#include "../util/StrFormat.h"
#include <boost/foreach.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/if.hpp>
#include <boost/lexical_cast.hpp>
#include <gtkmm/stock.h>
#include <gtkmm/treeviewcolumn.h>
#include <gtkmm/treerowreference.h>
#include "uftt-48x48.png.h"
#include <gdkmm/pixbufloader.h>
#include <limits.h>
#include <gtkmm/window.h>


TaskList::TaskList(UFTTSettingsRef _settings, Glib::RefPtr<Gtk::UIManager> uimanager_ref_)
: settings(_settings),
  uimanager_ref(uimanager_ref_),
  last_completion(boost::posix_time::neg_infin),
  last_notification(boost::posix_time::neg_infin)
{
	this->add(task_list_treeview);
	this->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	task_list_liststore = Gtk::ListStore::create(task_list_columns);
//	task_list_treeview.set_model(SortableTreeDragDest<Gtk::ListStore>::create(task_list_liststore)); // FIXME: Enabling this causes Gtk to give a silly warning
	task_list_treeview.set_model(task_list_liststore);
	#define ADD_TV_COL_SORTABLE(tv, title, column) (tv).get_column((tv).append_column((title), (column)) - 1)->set_sort_column((column));
	ADD_TV_COL_SORTABLE(task_list_treeview, "Status"        , task_list_columns.status_string);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Time Elapsed"  , task_list_columns.time_elapsed);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Time Remaining", task_list_columns.time_remaining);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Transferred"   , task_list_columns.transferred);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Total Size"    , task_list_columns.total_size);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Speed"         , task_list_columns.speed);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Queue"         , task_list_columns.queue);
	ADD_TV_COL_SORTABLE(task_list_treeview, "User Name"     , task_list_columns.user_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Share Name"    , task_list_columns.share_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Host Name"     , task_list_columns.host_name);
	ADD_TV_COL_SORTABLE(task_list_treeview, "Protocol"      , task_list_columns.protocol);
	ADD_TV_COL_SORTABLE(task_list_treeview, "URL"           , task_list_columns.url);
	#undef ADD_TV_COL_SORTABLE
	task_list_treeview.set_headers_clickable(true);
	task_list_treeview.set_rules_hint(true);
	task_list_treeview.set_search_column(task_list_columns.share_name);
	task_list_liststore->set_sort_column(task_list_columns.share_name, Gtk::SORT_ASCENDING);
	BOOST_FOREACH(Gtk::TreeViewColumn* column, task_list_treeview.get_columns()) {
		column->set_reorderable(true);
		column->set_resizable(true);
	}
	task_list_treeview.set_enable_search(true);
	task_list_treeview.set_rubber_banding(true);
	task_list_treeview.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	task_list_treeview.signal_row_activated().connect(boost::bind(&TaskList::execute_selected_tasks, this));
	task_list_treeview.signal_button_press_event().connect(
		sigc::mem_fun(this, &TaskList::on_task_list_treeview_signal_button_press_event), false);


	Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create("png");
	loader->write(uftt_48x48_png, sizeof(uftt_48x48_png));
	loader->close();
	ufft_icon = loader->get_pixbuf();


	/* Create actions */
	Glib::RefPtr<Gtk::ActionGroup> actiongroup_ref(Gtk::ActionGroup::create("UFTT"));
	Glib::RefPtr<Gtk::Action> action; // Only used when we need to add an accel_path to a menu-item

	/* View menu */
	action = Gtk::Action::create("ViewClearTaskList", Gtk::Stock::CLEAR, "_Clear Completed Tasks");
	action->set_sensitive(false);
	actiongroup_ref->add(action, boost::bind(&TaskList::cleanup, this));
	action->set_accel_path("<UFTT>/MainWindow/MenuBar/View/ClearTaskList");

	/* Task menu */
	action = Gtk::Action::create("TaskMenu", "_Task");
	action->set_sensitive(false);
	actiongroup_ref->add(action);

	action = Gtk::Action::create("TaskPause", Gtk::Stock::MEDIA_PAUSE);
	action->set_sensitive(false);
	actiongroup_ref->add(action);

	action = Gtk::Action::create("TaskResume", Gtk::Stock::MEDIA_PLAY,  "_Resume");
	action->set_sensitive(false);
	actiongroup_ref->add(action);

	action = Gtk::Action::create("TaskCancel", Gtk::Stock::MEDIA_STOP,  "_Cancel");
	action->set_sensitive(false);
	actiongroup_ref->add(action);

	action = Gtk::Action::create(
		"TaskExecute",
		Gtk::Stock::EXECUTE,
		"Open",
		"Opens the download using the default action"
	);
	action->set_sensitive(false);
	actiongroup_ref->add(
		action,
		boost::bind(&TaskList::execute_selected_tasks, this)
	);
	action = Gtk::Action::create(
		"TaskOpenContainingFolder",
		Gtk::Stock::OPEN,
		"Open containing folder",
		"Opens the folder where the task was downloaded to"
	);
	action->set_sensitive(false);
	actiongroup_ref->add(
		action,
		boost::bind(&TaskList::open_folder_selected_tasks, this)
	);

	uimanager_ref->insert_action_group(actiongroup_ref);

	task_list_treeview.get_selection()->signal_changed().connect(
		boost::bind(&TaskList::on_task_list_treeview_selection_signal_changed, this)
	);

	task_list_treeview.get_model()->signal_row_inserted().connect(
		boost::bind(&TaskList::on_task_list_treeview_signal_row_inserted_deleted, this)
	);
	task_list_treeview.get_model()->signal_row_deleted().connect(
		boost::bind(&TaskList::on_task_list_treeview_signal_row_inserted_deleted, this)
	);
}

void TaskList::on_task_list_treeview_signal_row_inserted_deleted() {
	uimanager_ref->get_action("/MenuBar/ViewMenu/ViewClearTaskList")->set_sensitive(
		task_list_treeview.get_model()->children().size() > 0
	);
}

void TaskList::on_task_list_treeview_selection_signal_changed() {
	bool has_selection = task_list_treeview.get_selection()->count_selected_rows() > 0;
	uimanager_ref->get_action("/MenuBar/TaskMenu")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/TaskMenu/TaskExecute")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/TaskMenu/TaskOpenContainingFolder")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/TaskMenu/TaskPause")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/TaskMenu/TaskResume")->set_sensitive(has_selection);
	uimanager_ref->get_action("/MenuBar/TaskMenu/TaskCancel")->set_sensitive(has_selection);
}

void TaskList::cleanup() {
	Gtk::TreeIter i = task_list_liststore->children().begin();
	while(i != task_list_liststore->children().end()) {
		TaskStatus status = i->get_value(task_list_columns.status);
		if(status == TASK_STATUS_COMPLETED || status == TASK_STATUS_ERROR) {
			i = task_list_liststore->erase(i);
		}
		else {
			i++;
		}
	}
}

void TaskList::execute_selected_tasks() {
	BOOST_FOREACH(Gtk::TreeModel::Path p, task_list_treeview.get_selection()->get_selected_rows()) {
		const Gtk::TreeModel::Row& row = *(task_list_liststore->get_iter(p));
		if(row[task_list_columns.status] == TASK_STATUS_COMPLETED) {
			Gtk::show_uri(Glib::wrap((GdkScreen*)NULL),
				Glib::filename_to_uri((((TaskInfo)row[task_list_columns.task_info]).path / ((TaskInfo)row[task_list_columns.task_info]).shareinfo.name).string()),
				GDK_CURRENT_TIME
			);
		}
	}
}

void TaskList::open_folder_selected_tasks() {
	BOOST_FOREACH(Gtk::TreeModel::Path p, task_list_treeview.get_selection()->get_selected_rows()) {
		const Gtk::TreeModel::Row& row = *(task_list_liststore->get_iter(p));
		std::cout << "taskinfo.path: '" << ((TaskInfo)row[task_list_columns.task_info]).path << "'" << std::endl;
		Gtk::show_uri(Glib::wrap((GdkScreen*)NULL),
			Glib::filename_to_uri(((TaskInfo)row[task_list_columns.task_info]).path.string()),
			GDK_CURRENT_TIME
		);
	}
}

bool TaskList::on_task_list_treeview_signal_button_press_event(GdkEventButton* event) {
	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		Gtk::TreeModel::Path  path;
		Gtk::TreeViewColumn* column;
		int    cell_x, cell_y;
		bool exists = task_list_treeview.get_path_at_pos((int)(event->x), (int)(event->y), path, column, cell_x, cell_y);
		if(exists) {
			if(!task_list_treeview.get_selection()->is_selected(path)) { // Clicked on a tree row, but it is not selected
				task_list_treeview.get_selection()->unselect_all();
				task_list_treeview.get_selection()->select(path);
			}
		}

		((Gtk::Menu*)uimanager_ref->get_widget("/TaskListPopup"))->popup(event->button, event->time);
		return true;
	}
	return false;
}

void TaskList::set_backend(UFTTCore* _core) {
	core = _core;
	core->connectSigNewTask(dispatcher.wrap(boost::bind(&TaskList::on_signal_new_task, this, _1)));
}

void TaskList::check_completed_tasks() {
	boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
	if(last_completion - last_notification > boost::posix_time::time_duration(boost::posix_time::seconds(5))) {
		notification = Gtk::Notification::create();
		TaskInfo latest = completed_tasks.back();
		completed_tasks.clear();
		completed_tasks.push_back(latest);
	}

	std::string body = "The following shares have been downloaded:\n";
	int i = 0;
	int max = 5;
	std::string spacer("  -  ");
	BOOST_FOREACH(TaskInfo& ti, completed_tasks) {
		body += spacer + ti.shareinfo.name + "\n";
		if(++i == max && completed_tasks.size() > max) {
			body += std::string() + spacer + "and " + boost::lexical_cast<std::string>(completed_tasks.size() - max) + " more...";
			break;
		}
	}

	notification->set_summary("Download complete");
	notification->set_body(body);
	notification->set_icon(ufft_icon);
	notification->set_urgency(Gtk::NOTIFY_URGENCY_NORMAL);
	notification->set_category("transfer.completed");
	notification->clear_actions();

	// FIXME: This only opens the most recently added share (or it's folder).
	//        Is this what we want our should we only show this when exactly
	//        one task has completed? There is a high probability that all
	//        tasks were downloaded to the same location anyway since they've
	//        completed so shortly after each other
	notification->add_action(
		boost::bind(
			&Gtk::show_uri,
			Glib::wrap((GdkScreen*)NULL),
			Glib::filename_to_uri((completed_tasks.front().path / completed_tasks.front().shareinfo.name).string()),
			GDK_CURRENT_TIME
		),
		"Open"
	);
	notification->add_action(
		dispatcher.wrap(
			boost::bind(
				&Gtk::show_uri,
				Glib::wrap((GdkScreen*)NULL),
				Glib::filename_to_uri(completed_tasks.front().path.string()),
				GDK_CURRENT_TIME
			)
		),
		"Open containing folder"
	);
	notification->add_action(
		dispatcher.wrap(
			boost::bind(
				&Gtk::Window::present,
				(Gtk::Window*)get_toplevel()
			)
		),
		"Show UFTT",
		"default"
	);

	last_notification = now;
	try{
		notification->show();
	} catch(Glib::Error /*e*/) {/* silently fail */}
}

void TaskList::on_signal_task_status(const boost::shared_ptr<Gtk::TreeModel::RowReference> rowref, const TaskInfo& info) {
	if(!*rowref) {
		std::cout << "Warning: BUG in  UFTTCore: calling SigTaskStatus after completion of task!" << std::endl;
		return;
	}
	Gtk::TreeModel::iterator i = task_list_liststore->get_iter(rowref->get_path());

	(*i)[task_list_columns.task_info]      = info;

	boost::posix_time::ptime current_time = boost::posix_time::microsec_clock::universal_time();
	boost::posix_time::time_duration time_elapsed = current_time-info.start_time;

	(*i)[task_list_columns.status]         = info.status;
	(*i)[task_list_columns.status_string]  = info.getTaskStatusString();
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(boost::posix_time::seconds(time_elapsed.total_seconds()));

	(*i)[task_list_columns.time_remaining] = "Unknown";
	if(info.speed > 0) {
		uint64 seconds_remaining = (((((info.size-info.transferred) << 0x04) + 0x08) / info.speed) >> 0x04); // round with 1/16 precision
		if(seconds_remaining <= std::numeric_limits<long>::max()) {
			if (info.transferred > 0 && info.size >= info.transferred && info.speed > 0) {
				(*i)[task_list_columns.time_remaining] =
					boost::posix_time::to_simple_string(
						boost::posix_time::time_duration(
							boost::posix_time::seconds(
								(long)seconds_remaining
							)
						)
					);
			}
		}
	}

	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	if(time_elapsed.total_seconds() > 0) {
		(*i)[task_list_columns.speed] = STRFORMAT("%s/s", StrFormat::bytes(info.speed));
	}
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user;
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.share_name]     = (info.isupload ? "U: " : "D: ") + info.shareinfo.name;

	if(info.status == TASK_STATUS_ERROR) {
		boost::shared_ptr<Gtk::Notification> notification = Gtk::Notification::create();
		notification->set_summary("Error during transfer");
		notification->set_body(
			std::string() + "An error occured while " +
			(info.isupload ? "uploading" : "downloading") +
			" \"" + info.shareinfo.name + "\":\n" +
			info.error_message
		);
		notification->set_icon(ufft_icon);
		notification->set_urgency(Gtk::NOTIFY_URGENCY_CRITICAL);
		notification->set_category("transfer.error");
		notification->add_action(
			dispatcher.wrap(
				boost::bind(
					&Gtk::Window::present,
					(Gtk::Window*)get_toplevel()
				)
			),
			"Show UFTT",
			"default"
		);
		try {
			notification->show();
		} catch(Glib::Error /*e*/) {/* silently fail */}
	}

	if(!info.isupload && info.status == TASK_STATUS_COMPLETED) { // Transfer done, explicitly set ETA to 00:00:00
		completed_tasks.push_back(info);
		last_completion = boost::posix_time::microsec_clock::universal_time();
		check_completed_tasks();

		(*i)[task_list_columns.time_remaining] = boost::posix_time::to_simple_string(boost::posix_time::time_duration(boost::posix_time::seconds(0)));
		if(settings->auto_clear_tasks_after >= boost::posix_time::seconds(0)) {
			Glib::signal_timeout().connect_seconds_once(
				boost::function<void(void)>(
					boost::lambda::if_then(
						boost::lambda::bind(
							&boost::shared_ptr<Gtk::TreeModel::RowReference>::operator*,
							rowref
						),
						boost::lambda::bind(
							&Gtk::ListStore::erase,
							task_list_liststore.operator->(),
							boost::lambda::bind(
								(Gtk::TreeModel::iterator (Gtk::TreeModel::*)(const Gtk::TreeModel::Path&))(&Gtk::TreeModel::get_iter),
								task_list_liststore.operator->(),
								boost::lambda::bind(
									&Gtk::TreeModel::RowReference::get_path,
									boost::lambda::bind(
										&boost::shared_ptr<Gtk::TreeModel::RowReference>::operator*,
										rowref
									)
								)
							)
						)
					)
				),
				settings->auto_clear_tasks_after.get().total_seconds()
			);
		}
	} // i may be invalid now

// FIXME: Something with auto-update
//	if (!info.isupload && info.status == "Completed") {}
}

void TaskList::on_signal_new_task(const TaskInfo& info) {
	Gtk::TreeModel::iterator i = task_list_liststore->append();
	(*i)[task_list_columns.task_info]      = info;
	// Task info (real values only filled in on_signal_task_status)
	(*i)[task_list_columns.status]         = info.status;
	(*i)[task_list_columns.status_string]  = "Waiting for peer";
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(boost::posix_time::time_duration(boost::posix_time::seconds(0)));
	(*i)[task_list_columns.time_remaining] = "Unknown";
	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	(*i)[task_list_columns.speed]          = "Unknown";
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user == "" ? "Anonymous" : info.shareinfo.user;
	(*i)[task_list_columns.share_name]     = info.shareinfo.name;
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.protocol]       = info.shareinfo.proto;
	(*i)[task_list_columns.url]            = STRFORMAT("%s://%s/%s", info.shareinfo.proto, info.shareinfo.host, info.shareinfo.name);

	// NOTE:
	//  1) We use a Gtk::RowReference and not an iterator, because RowReferences
	//     are better suited for the task at hand. They are designed to remain
	//     valid as long as the row they reference is present in the Model,
	//     regardless of wether or not rows are added, removed, reordered etc.
	//  2) We use a pointer because the RowReference may otherwise get copied
	//     somewhere along the line (in particular it seems when the bound
	//     function object is invoked?). This poses a problem because when
	//     a RowReference has become invalid (e.g. after clear() of the Model)
	//     and subsequently it's copied it will spew an ugly warning (which)
	//     we don't want.
	boost::shared_ptr<Gtk::TreeModel::RowReference> rowref(new Gtk::TreeModel::RowReference(task_list_liststore, Gtk::TreePath(i)));
	boost::function<void(const TaskInfo&)> handler =
		dispatcher.wrap(boost::bind(&TaskList::on_signal_task_status, this, rowref, _1));
	core->connectSigTaskStatus(info.id, handler);
}
