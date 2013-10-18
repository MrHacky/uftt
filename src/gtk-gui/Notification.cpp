#include "Notification.h"
#include <boost/lexical_cast.hpp>
#include <iostream>

// NOTE: This namespace trickery is mostly for doxygen's benefit, it easily
//       gets confused... This is also why we don't do "using namespace Glib;"
namespace Gtk {

	/* Static variables */
	boost::mutex  Notification::n_instances_mutex;
	int           Notification::n_instances = 0;
	Glib::ustring Notification::application_name = "Notification";

	// keep this here to ensure libnotify global (de)init is called only once
	// (and from a single thread)
	boost::shared_ptr<Notification> global_instance(Notification::create());

	#ifndef HAVE_LIBNOTIFY
		// Be compatible when libnotify is not available
		#error "FIXME: Dummy implementation of libnotify"
	#endif


// FIXME: CLEAR DEBUGGING MESSAGES
	/******************************
	 ** Constructors (public)    **
	 ******************************/
	boost::shared_ptr<Notification> Notification::create() {
		return boost::shared_ptr<Notification>(new Notification());
	}

	boost::shared_ptr<Notification> Notification::create(const Glib::ustring summary_, const Glib::ustring body_, const Gtk::StockID icon_) {
		return boost::shared_ptr<Notification>(new Notification(summary_, body_, icon_));
	}

	boost::shared_ptr<Notification> Notification::create(const Glib::ustring summary_, const Glib::ustring body_, const Glib::RefPtr<Gdk::Pixbuf> icon_) {
		return boost::shared_ptr<Notification>(new Notification(summary_, body_, icon_));
	}

	boost::shared_ptr<Notification> Notification::create(Gtk::Widget& widget_, const Glib::ustring summary_, const Glib::ustring body_, const Gtk::StockID icon_) {
		return boost::shared_ptr<Notification>(new Notification(widget_, summary_, body_, icon_));
	}

	boost::shared_ptr<Notification> Notification::create(Gtk::Widget& widget_, const Glib::ustring summary_, const Glib::ustring body_, const Glib::RefPtr<Gdk::Pixbuf> icon_) {
		return boost::shared_ptr<Notification>(new Notification(widget_, summary_, body_, icon_));
	}




	/******************************
	 ** Constructors (private)   **
	 ******************************/

	Notification::Notification()
	: next_action_id(0)
	, notify_notification(NULL)
	{
		check_libnotify();
		initialize_notification();
	}

	Notification::Notification(const Glib::ustring summary_, const Glib::ustring body_, const Gtk::StockID icon_)
	: next_action_id(0)
	, notify_notification(NULL)
	, summary(summary_)
	, body(body_)
	, icon(icon_)
	{
		check_libnotify();
		initialize_notification();
	}

	Notification::Notification(const Glib::ustring summary_, const Glib::ustring body_, const Glib::RefPtr<Gdk::Pixbuf> icon_)
	: next_action_id(0)
	, notify_notification(NULL)
	, summary(summary_)
	, body(body_)
	, icon_pixbuf(icon_)
	{
		check_libnotify();
		initialize_notification();
	}

	Notification::Notification(Gtk::Widget& widget_, const Glib::ustring summary_, const Glib::ustring body_, const Gtk::StockID icon_)
	: next_action_id(0)
	, notify_notification(NULL)
	, summary(summary_)
	, body(body_)
	, icon(icon_)
	{
		check_libnotify();
		initialize_notification();
	}

	Notification::Notification(Gtk::Widget& widget_, const Glib::ustring summary_, const Glib::ustring body_, const Glib::RefPtr<Gdk::Pixbuf> icon_)
	: next_action_id(0)
	, notify_notification(NULL)
	, summary(summary_)
	, body(body_)
	, icon_pixbuf(icon_)
	{
		check_libnotify();
		initialize_notification();
	}




	/******************************
	 ** Destructor               **
	 ******************************/

	Notification::~Notification() {
		if(notify_notification != NULL) {
			if(g_signal_handler_is_connected(this->notify_notification, this->signal_closed_callback_id))
				g_signal_handler_disconnect(this->notify_notification, this->signal_closed_callback_id);

			g_object_unref(this->notify_notification);
			this->notify_notification = NULL;
		}
		boost::mutex::scoped_lock lock(n_instances_mutex);
		n_instances--;
		if(n_instances == 0) {
			notify_uninit();
		}
	}



	/******************************
	 ** Public static functions  **
	 ******************************/

	void Notification::set_application_name(const Glib::ustring application_name) {
		boost::mutex::scoped_lock lock(n_instances_mutex);
		Notification::application_name = application_name;
		global_instance->reinitialize();
	}

	Glib::ustring Notification::get_application_name() {
		boost::mutex::scoped_lock lock(n_instances_mutex);
		return Notification::application_name;
	}

	Glib::ListHandle<Glib::ustring> Notification::get_server_capabilities() {
		GList* list = notify_get_server_caps();
		if(list == NULL) {
			throw Glib::Error();
		}
		return Glib::ListHandle<Glib::ustring>(list, Glib::OWNERSHIP_DEEP);
	}

	struct Notification::ServerInfo Notification::get_server_info() {
		struct ServerInfo info;
		char *ret_name, *ret_vendor, *ret_version, *ret_spec_version;

		bool got_info = notify_get_server_info(
			&ret_name,
			&ret_vendor,
			&ret_version,
			&ret_spec_version
		);

		if(!got_info) {
			throw Glib::Error();
		}

		info.name         = ret_name;
		info.vendor       = ret_vendor;
		info.version      = ret_version;
		info.spec_version = ret_spec_version;

		g_free(ret_name);
		g_free(ret_vendor);
		g_free(ret_version);
		g_free(ret_spec_version);

		return info;
	}

	/******************************
	 ** Public functions         **
	 ******************************/

	void Notification::set_summary(const Glib::ustring summary) {
		set_contents(summary, this->body, this->icon);
	}

	Glib::ustring Notification::get_summary() const {
		return summary;
	}

	void Notification::set_body(const Glib::ustring body) {
		set_contents(this->summary, body, this->icon);
	}

	Glib::ustring Notification::get_body() const {
		return body;
	}

	void Notification::set_icon(const Gtk::StockID icon) {
		this->icon_pixbuf.reset();
		set_contents(this->summary, this->body, icon);
	}

	Glib::RefPtr<Gdk::Pixbuf> Notification::get_icon() const {
		if(icon_pixbuf)
			return icon_pixbuf;

		Gtk::Image img;
		Gtk::IconSize size(Gtk::ICON_SIZE_DIALOG);
		Gtk::Stock::lookup(icon, size, img);
		return img.get_pixbuf();
	}

	void Notification::set_icon(const Glib::RefPtr<Gdk::Pixbuf> icon) {
		this->icon = Gtk::StockID();
		this->icon_pixbuf = icon;
		notify_notification_set_icon_from_pixbuf(
			this->notify_notification,
			this->icon_pixbuf->gobj()
		);
	}

	void Notification::set_category(const Glib::ustring category) {
		this->category = category;
		notify_notification_set_category(
			this->notify_notification,
			this->category.c_str()
		);
	}

	Glib::ustring Notification::get_category() {
		return this->category;
	}

	void Notification::set_contents(const Glib::ustring summary, const Glib::ustring body, const Gtk::StockID icon) {
		this->summary = summary;
		this->body = body;
		this->icon = icon;
		if(this->icon) {
			//fixme
		}
		notify_notification_update(
			this->notify_notification,
			this->summary.c_str(),
			this->body.size() > 0 ? this->body.c_str()              : NULL,
			this->icon            ? this->icon.get_string().c_str() : NULL
		);
	}

	void Notification::set_urgency(NotifyUrgency urgency) {
		this->urgency = urgency;
		notify_notification_set_urgency(
			this->notify_notification,
			this->urgency
		);
	}

	NotifyUrgency Notification::get_urgency() {
		return this->urgency;
	}

	void Notification::show() {
		GError* error = NULL;
		bool shown = notify_notification_show(this->notify_notification, &error);
		if(error != NULL) {
			Glib::Error::throw_exception(error);
		}
		if(!shown) {
			throw Glib::Error();
		}
	}

	void Notification::clear_actions() {
		notify_notification_clear_actions(this->notify_notification);
	}

	void Notification::notify_action_free_function(gpointer user_data) {
		sigc::slot<void>* callback = (sigc::slot<void>*)user_data;
		delete callback;
	}

	void Notification::notify_action_callback(NotifyNotification *notification, gchar *action, gpointer user_data) {
		sigc::slot<void>* callback = (sigc::slot<void>*)user_data;
		(*callback)();
	}

	void Notification::add_action(const sigc::slot<void> callback, const Glib::ustring label, const Glib::ustring identifier) {
		sigc::slot<void>* callback_copy = new (sigc::slot<void>)(callback);
		Glib::ustring action_id = identifier;
		if(identifier.empty())
			action_id = boost::lexical_cast<Glib::ustring>(next_action_id++);

		notify_notification_add_action(
			this->notify_notification,
			action_id.c_str(),
			label.c_str(),
			&Notification::notify_action_callback,
			callback_copy,
			&notify_action_free_function
		);
	}

	/*

	void Widget_signal_parent_changed_callback(GtkWidget* self, GtkWidget* p0,void* data)
	{
		using namespace Gtk;
		typedef sigc::slot< void,Widget* > SlotType;

		// Do not try to call a signal on a disassociated wrapper.
		if(Glib::ObjectBase::_get_current_wrapper((GObject*) self))
		{
			try
			{
				if(sigc::slot_base *const slot = Glib::SignalProxyNormal::data_to_slot(data))
					(*static_cast<SlotType*>(slot))(Glib::wrap(p0)
	);
			}
			catch(...)
			{
				Glib::exception_handlers_invoke();
			}
		}
	}

	const Glib::SignalProxyInfo Widget_signal_parent_changed_info =
	{
		"parent_set",
		(GCallback) &Widget_signal_parent_changed_callback,
		(GCallback) &Widget_signal_parent_changed_callback
	};

	Glib::SignalProxy1< void,Widget* > Widget::signal_parent_changed()
	{
		return Glib::SignalProxy1< void,Widget* >(this, &Widget_signal_parent_changed_info);
	}

	*/

	void Notification::signal_closed_callback(NotifyNotification* notify_notification, Notification* notification) {
		notification->signal_closed_((Gtk::Notification::ClosedReason)notify_notification_get_closed_reason(notify_notification));
	}

	/* Leave this here in case we want to try Glib::SignalProxy again someday
	const Glib::SignalProxyInfo Notification_signal_closed_info =
	{
		"closed",
		(GCallback) &Notification_signal_closed_callback,
		(GCallback) &Notification_signal_closed_callback
	};

	Glib::SignalProxy1<void,int> Notification::signal_closed() {
		std::cerr << "+++ Notification::signal_closed" << std::endl;
		return Glib::SignalProxy1<void, int>(NULL, &Notification_signal_closed_info);
	}
	*/

	sigc::signal<void, Gtk::Notification::ClosedReason>& Notification::signal_closed() {
		// Defer connecting to signal handler until it's actually used
		if(!g_signal_handler_is_connected(this->notify_notification, this->signal_closed_callback_id)) {
			this->signal_closed_callback_id = g_signal_connect(
				G_OBJECT(this->notify_notification),
				"closed",
				G_CALLBACK(Notification::signal_closed_callback),
				this
			);
		}
		return this->signal_closed_;
	}


	/******************************
	 ** Private functions        **
	 ******************************/

	void Notification::check_libnotify() {
		{
			boost::mutex::scoped_lock lock(n_instances_mutex);
			if(n_instances == 0) {
				if(notify_is_initted()) {
					throw Glib::Error(); // FIXME: "libnotitify initialised by outside source"
				}
				notify_init(Notification::application_name.c_str());
			}
			n_instances++;
		}
	}

	void Notification::initialize_notification() {
		if(this->summary == "") {
			this->summary = application_name + " says hi!";
		}

		this->notify_notification = notify_notification_new(
			this->summary.c_str(),
			this->body.size() > 0 ? this->body.c_str()              : NULL,
			this->icon            ? this->icon.get_string().c_str() : NULL
		);

		if(!this->notify_notification) {
			throw Glib::Error(); // FIXME: "Error while constructing notification";
		}
	}

	void Notification::reinitialize() {
		// WARNING:
		//   only the global instance may be reinitialized, and only
		//   when the caller holds the instance lock
		if(n_instances > 1) {
			std::cerr << "error!" << std::endl;
			throw Glib::Error(); // FIXME: "You should not call set_application_name() while Notifications are still active"
		}
		if(notify_is_initted()) {
			notify_uninit();
		}
		notify_init(Notification::application_name.c_str());

	}

} // namespace Gtk
