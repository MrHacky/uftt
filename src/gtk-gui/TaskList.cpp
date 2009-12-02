#include "TaskList.h"
#include "../util/StrFormat.h"
#include <boost/foreach.hpp>
#include <gtkmm/treeviewcolumn.h>


TaskList::TaskList() {
	this->add(task_list_treeview);
	this->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	task_list_liststore = Gtk::ListStore::create(task_list_columns);
//	task_list_treeview.set_model(SortableTreeDragDest<Gtk::ListStore>::create(task_list_liststore)); // FIXME: Enabling this causes Gtk to give a silly warning
	task_list_treeview.set_model(task_list_liststore);
	#define ADD_TV_COL_SORTABLE(tv, title, column) (tv).get_column((tv).append_column((title), (column)) - 1)->set_sort_column((column));
	ADD_TV_COL_SORTABLE(task_list_treeview, "Status"        , task_list_columns.status);
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
	task_list_treeview.signal_button_press_event().connect(
		sigc::mem_fun(this, &TaskList::on_task_list_treeview_signal_button_press_event), false);
}

void TaskList::set_popup_menu(Gtk::Menu* _popup_menu) {
	popup_menu = _popup_menu;
}

bool TaskList::on_task_list_treeview_signal_button_press_event(GdkEventButton* event) {
	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3) && (popup_menu != NULL)) {
		popup_menu->popup(event->button, event->time);
		return true;
	}
	return false;
}

void TaskList::set_backend(UFTTCoreRef _core) {
	core = _core;
	core->connectSigNewTask(dispatcher.wrap(boost::bind(&TaskList::on_signal_new_task, this, _1)));
}

void TaskList::on_signal_task_status(const Gtk::TreeModel::iterator i, const boost::posix_time::ptime start_time, const TaskInfo& info) {
	boost::posix_time::ptime current_time = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration time_elapsed = current_time-start_time;

	(*i)[task_list_columns.status]         = info.status;
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(time_elapsed);
	if (info.size > 0 && info.size >= info.transferred && info.transferred > 0) {
		(*i)[task_list_columns.time_remaining] = 
			boost::posix_time::to_simple_string(
				boost::posix_time::time_duration(
					boost::posix_time::seconds(
						((info.size-info.transferred) * time_elapsed.total_seconds() / info.transferred)
					)
				)
			);
	}
	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	if(time_elapsed.total_seconds() > 0) {
		(*i)[task_list_columns.speed] = STRFORMAT("%s/s", StrFormat::bytes(info.transferred/time_elapsed.total_seconds()));
	}
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user;
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.share_name]     = (info.isupload ? "U: " : "D: ") + info.shareinfo.name;

// FIXME: Something with auto-update
//	if (!info.isupload && info.status == "Completed") {} 
}

void TaskList::on_signal_new_task(const TaskInfo& info) {
	Gtk::TreeModel::iterator i = task_list_liststore->append();
	// Task info (real values only filled in on_signal_task_status)
	(*i)[task_list_columns.status]         = "Waiting for peer";
	(*i)[task_list_columns.time_elapsed]   = boost::posix_time::to_simple_string(boost::posix_time::time_duration(boost::posix_time::seconds(0)));
	(*i)[task_list_columns.time_remaining] = "Unknown";
	(*i)[task_list_columns.transferred]    = StrFormat::bytes(info.transferred);
	(*i)[task_list_columns.total_size]     = StrFormat::bytes(info.size);
	(*i)[task_list_columns.speed]          = "Unknown";
	(*i)[task_list_columns.queue]          = info.queue;
	// Share info
	(*i)[task_list_columns.user_name]      = info.shareinfo.user;
	(*i)[task_list_columns.share_name]     = info.shareinfo.name;
	if(info.shareinfo.name == "") (*i)[task_list_columns.share_name] = "Anonymous";
	(*i)[task_list_columns.host_name]      = info.shareinfo.host;
	(*i)[task_list_columns.protocol]       = info.shareinfo.proto;
	(*i)[task_list_columns.url]            = STRFORMAT("%s://%s/%s", info.shareinfo.proto, info.shareinfo.host, info.shareinfo.name);

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	// NOTE: Gtk::ListStore guarantees that iterators are valid as long as the
	// row they reference is valid.
	// See http://library.gnome.org/devel/gtkmm/unstable/classGtk_1_1TreeIter.html#_details
	boost::function<void(const TaskInfo&)> handler =
		dispatcher.wrap(boost::bind(&TaskList::on_signal_task_status, this, i, starttime, _1));
	core->connectSigTaskStatus(info.id, handler);
}
