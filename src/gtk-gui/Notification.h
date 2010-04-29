#ifndef NOTIFY_NOTIFICATION_H
#define NOTIFY_NOTIFICATION_H

#include <boost/thread/mutex.hpp>

#include <gdkmm/screen.h>

#include <glibmm/arrayhandle.h>
#include <glibmm/error.h>
#include <glibmm/listhandle.h>
#include <glibmm/ustring.h>

#include <gtkmm/statusicon.h>
#include <gtkmm/stock.h>

#include <sigc++/functors/slot.h>

// Pretend we're part of Gtk (we should be, IMHO)
namespace Gtk {
	#ifndef HAVE_LIBNOTIFY
		// Be compatible when libnotify is not available
		#error "FIXME: Dummy implementation of libnotify"
	#else
		#include <libnotify/notification.h>
		#include <libnotify/notify-enum-types.h>
		#include <libnotify/notify.h>
	#endif

	/**
	 * Notification provides a C++ interface to libnotify. Libnotify is a library
	 * that sends desktop notifications to a notification daemon, as defined in
	 * the Desktop Notifications spec. These notifications can be used to inform
	 * the user about an event or display some form of information without
	 * getting in the user's way.
	 *
	 * @note This code is <strong>NOT</strong> the same, taken from or based upon
	 *       <a href='http://git.gnome.org/browse/libnotifymm/'>libnotifymm</a>.
	 *       In fact we only found out that libnotifymm existed at all after this
	 *       interface was mostly completed, which is a shame since it could have
	 *       saved us a lot of work.
	 *
	 * @note We have however decided to keep this interface in favor of
	 *       libnotifymm for a number of reasons:
	 *       <ul>
	 *         <li>
	 *           The work was done and it would be a shame to put it to waste.
	 *           Our implementation is on par with libnotifymm, and even offers
	 *           a few more convenience functions and overloads that libnotify
	 *           does not offer. Notable examples include get_() methods for most
	 *           set_() fuctions (libnotifymm does not offer this) and separate
	 *           functions to set the summary, body and icon (libnotifymm only
	 *           supports the update() method).
	 *         </li>
	 *         <li>
	 *           Further libnotifymm seems to be a rather unknown library,
	 *           currently missing from the package databases of several Linux
	 *           distributions. In particular we did not find it present in
	 *           Gentoo, Fedora and Arch Linux. It was present however at least
	 *           in Debian and Ubuntu. Still, this lack of support means that it
	 *           will be more difficult for people to use (i.e. compile and run)
	 *           UFTT on those distro's.
	 *         </li>
	 *         <li>
	 *           Finally it is at present unclear to us wether or not either
	 *           libnotify or libnotifymm will work under environments other than
	 *           Gnome (Gtk) / Linux (e.g. Windows Xp/Vista/Se7en or Mac OS 9/10).
	 *           By implementing our own wrapper around libnotify we may more
	 *           easily provide our own implementation of the core libnotify
	 *           functionality (set summary/body, show notification etc), and
	 *           provide support for notifications on on those alternate
	 *           platforms.
	 *         </li>
	 *       </ul>
	 */
	class Notification // FIXME: public Glib::Object
	{
		/* Public static functions */
		public:
			/**
			 * Sets the name of the application.
			 * This reinitialises libnotify and hence most likely invalidates
			 * the internal state of all Notification objects. You should call this
			 * function at the start of your application, <strong>before</strong> any
			 * Notification object is instantiated.
			 *
			 * @note The reason for this is that although libnotify requires an
			 *       application name to be set, however this may only be done
			 *       when initialising libnotify. Because we ideally want to
			 *       initialise libnotify only once, and not for every notification,
			 *       you should try to use the same application name (or none)
			 *       throughout your application.
			 *       If you do not set the application name a default name will be
			 *       provided for you.
			 * @throw Glib::Error(), if an error occurs during the
			 *       (re)intialisation of libnotify.
			 */
			static void set_application_name(const Glib::ustring application_name);

			/**
			 * Returns the name of the application as set by set_application_name().
			 * @return The name of the application.
			 */
			static Glib::ustring get_application_name();

			/**
			 * Returns the capabilities of the notification server.
			 * @return A list of capability strings.
			 * @throw Glib::Error
			 */
			static Glib::ListHandle<Glib::ustring> get_server_capabilities();

			/**
			 * Struct containing a set of strings describing the server.
			 */
			struct ServerInfo {
				Glib::ustring name;         /**< The product name of the server.      */
				Glib::ustring spec_version; /**< The specification version supported. */
				Glib::ustring vendor;       /**< The vendor.                          */
				Glib::ustring version;      /**< The server version.                  */
			};

			/**
			 * Returns the server notification information.
			 * @return A struct ServerInfo.
			 */
			static struct ServerInfo get_server_info();



			/* Constructors */

			/**
			 * Creates a new Notification.
			 */
			static boost::shared_ptr<Notification> create();

			/**
			 * @anchor main_notification_constructor_description
			 * Creates a new Notification.
			 *
			 * @anchor notify_summary_description
			 * @param summary is a single line overview of the notification. For
			 *        instance, "You have mail" or "A friend has come online". It
			 *        should generally not be longer than 40 characters, though this
			 *        is not a requirement, and server implementations should word
			 *        wrap if necessary.
			 * @anchor notify_body_description
			 * @param body is a multi-line body of text. Each line is a paragraph,
			 *        server implementations are free to word wrap them as they see
			 *        fit. The body may contain simple markup as specified in Markup.
			 *        <br>
			 *        <em>
			 *        See also <a href='
			 *        http://www.galago-project.org/specs/notification/0.9/x161.html'>
			 *        the notification specification</a> for supported markup
			 *        elements.
			 *        </em>
			 * @anchor notify_icon_description
			 * @param icon is the notification icon.
			 *        <br>
			 *        <em>
			 *        Note that Notification currently supports only Gtk::StockID
			 *        and pixbuf icons, but does not provide for icon names
			 *        represented either as a URI or a name in a
			 *        freedesktop.org-compliant icon theme (as required by the
			 *        notification specification).
			 *        </em>
			 */
			static boost::shared_ptr<Notification> create(const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());

			/**
			 * @param summary is a single line overview of the notification.
			 * @param body is a multi-line body of text.
			 * @param icon is the notification icon.
			 *
			 * Please refer to the @ref main_notification_constructor_description for
			 * more information on the parameters.
			 */
			static boost::shared_ptr<Notification> create(const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());

			/**
			 * @param widget is the widget to attach to.
			 * @param summary is a single line overview of the notification.
			 * @param body is a multi-line body of text.
			 * @param icon is the notification icon.
			 *
			 * Please refer to the @ref main_notification_constructor_description for
			 * more information on the parameters.
			 *
			 * @see attach_to(Gtk::Widget& widget)
			 */
			static boost::shared_ptr<Notification> create(Gtk::Widget& widget, const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());

			/**
			 * @param widget is the widget to attach to.
			 * @param summary is a single line overview of the notification.
			 * @param body is a multi-line body of text.
			 * @param icon is the notification icon.
			 *
			 * Please refer to the @ref main_notification_constructor_description for
			 * more information on the parameters.
			 *
			 * @see attach_to(Gtk::Widget& widget)
			 */
			static boost::shared_ptr<Notification> create(Gtk::Widget& widget, const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());

			/**
			 * @param statusicon is the statusicon to attach to.
			 * @param summary is a single line overview of the notification.
			 * @param body is a multi-line body of text.
			 * @param icon is the notification icon.
			 *
			 * Please refer to the @ref main_notification_constructor_description for
			 * more information on the parameters.
			 *
			 * @see attach_to(const Gtk::StatusIcon& statusicon)
			 */
			static boost::shared_ptr<Notification> create(const Glib::RefPtr<Gtk::StatusIcon> statusicon, const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());

			/**
			 * @param statusicon is the statusicon to attach to.
			 * @param summary is a single line overview of the notification.
			 * @param body is a multi-line body of text.
			 * @param icon is the notification icon.
			 *
			 * Please refer to the @ref main_notification_constructor_description for
			 * more information on the parameters.
			 *
			 * @see attach_to(const Gtk::StatusIcon& statusicon)
			 */
			static boost::shared_ptr<Notification> create(const Glib::RefPtr<Gtk::StatusIcon> statusicon, const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());



			/* Public functions */
			~Notification();

			/**
			 * Sets the summary for the notification.
			 * @see @ref notify_summary_description
			 * @param summary is a single line overview of the notification.
			 */
			void set_summary(const Glib::ustring summary);

			/**
			 * @see @ref notify_summary_description
			 * @return the summary for the notification.
			 */
			Glib::ustring get_summary() const;

			/**
			 * Sets the body for the notification.
			 * @see @ref notify_body_description
			 * @param body is a multi-line body of text.
			 */
			void set_body(const Glib::ustring body);

			/**
			 * @see @ref notify_body_description
			 * @return the body for the notification.
			 */
			Glib::ustring get_body() const;

			/**
			 * @overload void set_icon(const Gtk::StockID icon)
			 */
			void set_icon(const Gtk::StockID icon);

			/**
			 * Sets the icon for the notification.
			 * @see @ref notify_icon_description
			 * @param icon is the notification icon.
			 */
			void set_icon(const Glib::RefPtr<Gdk::Pixbuf> icon);

			/**
			 * @see @ref notify_icon_description
			 * @return the current icon for the notification, if any.
			 */
			Glib::RefPtr<Gdk::Pixbuf> get_icon() const;

			/**
			 * This function is the combination of set_summary(), set_body() and
			 * set_icon() functions.
			 * @note This is the actual function that is called by the before
			 *       mentioned three functions, and maps exactly to
			 *       notify_notification_update()
			 * @see @ref main_Notification_constructor_description for a description
			 *      of the parameters.
			 */
			void set_contents(const Glib::ustring summary = "", const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());

			/**
			 * Attaches the Notification to a specific widget.
			 * @param widget is the widget to attach to.
			 */
			void attach_to(Gtk::Widget& widget);

			/**
			 * Attaches the Notification to a specific statusicon.
			 * @param statusicon is the statusicon to attach to.
			 */
			void attach_to(const Gtk::StatusIcon& statusicon);

			/**
			 * Sets the geometry hints on the notification. This sets the screen
			 * the notification should appear on and the X, Y coordinates it should
			 * point to, if the particular notification supports X, Y hints.
			 * @param screen is the Gdk::Screen the notification should appear on.
			 * @param x is the X coordinate to point to.
			 * @param y is the Y coordinate to point to.
			 */
			void set_geometry_hints(Glib::RefPtr<Gdk::Screen> screen, int x, int y);

			/**
			 * Tells the notification server to display the notification on the
			 * screen.
			 * @throw Glib::Error().
			 */
			void show();

			/**
			 * Tells the notification server to hide the notification on the screen.
			 * @throw Glib::Error().
			 */
			void hide();

			/**
			 * Sets the timeout of the notification. To set the default time, pass
			 * NOTIFY_EXPIRES_DEFAULT as timeout. To set the notification to never
			 * expire, pass NOTIFY_EXPIRES_NEVER.
			 * @param timeout is the timeout in milliseconds.
			 */
			void set_timeout(int timeout = NOTIFY_EXPIRES_DEFAULT);

			/**
			 * @return the timeout of the Notification as set by set_timeout().
			 */
			int  get_timeout();

			/**
			 * Sets the category of this notification. This can be used by the
			 * notification server to filter or display the data in a certain way.
			 * <br>
			 * <em>
			 * See also <a href='
			 * http://www.galago-project.org/specs/notification/0.9/x211.html'>
			 * the notification specification</a> for a list of possible categories.
			 * </em>
			 * @param category is the category.
			 */
			void          set_category(const Glib::ustring category);

			/**
			 * @return the current category of the Notification (if any) as set by
			 *         set_category().
			 */
			Glib::ustring get_category();

			/**
			 * Sets the urgency level of this notification. This defines the importance of
			 * the notification. For example, "Joe Bob signed on" would be a low urgency.
			 * "You have new mail" or "A USB device was unplugged" would be a normal
			 * urgency. "Your computer is on fire" would be a critical urgency.
			 * @param urgency is an NotifyUrgency level.
			 * @see NotifyUrgency
			 *
			 * FIXME: doxygen can't find the notification headers, so
			 *        \@see NotifyUrgency doesn't work...
			 */
			void          set_urgency(const NotifyUrgency urgency);

			/**
			 * @return the urgency level of this notification as set by set_urgency().
			 */
			NotifyUrgency get_urgency();

			/**
			 * Sets a hint with an integer value.
			 * @param key is a string identifying the hint.
			 * @param value is the hint's value.
			 */
			void set_hint(const Glib::ustring key, const unsigned int            value);

			/**
			 * Sets a hint with a double value.
			 * @param key is a string identifying the hint.
			 * @param value is the hint's value.
			 */
			void set_hint(const Glib::ustring key, const double                  value);

			/**
			 * Sets a hint with a string value.
			 * @param key is a string identifying the hint.
			 * @param value is the hint's value.
			 */
			void set_hint(const Glib::ustring key, const Glib::ustring           value);

			/**
			 * Sets a hint with a byte value.
			 * @param key is a string identifying the hint.
			 * @param value is the hint's value.
			 */
			void set_hint(const Glib::ustring key, const char                    value);

			/**
			 * Sets a hint with a byte array value value.
			 * @param key is a string identifying the hint.
			 * @param value is the hint's value.
			 */
			void set_hint(const Glib::ustring key, const Glib::ArrayHandle<char> value);

			/**
			 * Clears all hints from the notification.
			 */
			void clear_hints();

			/**
			 * Adds an action to a notification. When the action is invoked, the
			 * specified callback function will be called. This functionality may not
			 * be implemented by the notification server, so you  should check if it
			 * is available before using it (see get_server_capabilities()). Also be
			 * aware that servers are free to ignore any action requested.
			 * Most servers render actions as  buttons in the notification popup.
			 *
			 * If you wish to provide a default action (usually invoked my clicking
			 * the notification) you should provide your own action identifier
			 * named "default" (leave the label empty).
			 *
			 * @param callback is the action's callback function.
			 * @param label is a human-readable action label.
			 * @param identifier is an optional action identifier.
			 * @see get_server_capabilities()
			 */
			void add_action(const sigc::slot<void> callback, const Glib::ustring label, const Glib::ustring identifier = "");

			/**
			 * Clears all actions from the notification.
			 */
			void clear_actions();

			/**
			 * Possible reasons for closing a Notification.
			 * @note It appears that on some notification servers (in particular on
			 *       the notifications daemon of the Galago project, version 0.4.0)
			 *       NOTIFICATION_CLOSED_DISMISSED is only be emitted when the user
			 *       dismisses the Notification by clicking in the body area of the
			 *       window (action "default"), <emph>not</emph> when clicking the
			 *       window's close button. Clicking the close button is interpreted
			 *       as letting the notification expire (NOTIFICATION_CLOSED_EXPIRED).
			 */
			typedef enum {
				NOTIFICATION_CLOSED_INVALID = 0,
				NOTIFICATION_CLOSED_EXPIRED = 1,
				NOTIFICATION_CLOSED_DISMISSED = 2,
				NOTIFICATION_CLOSED_PROGRAMMATICALY = 3,
				NOTIFICATION_CLOSED_RESERVED = 4
			} ClosedReason;

			/**
			 * Adds the ability to receive a signal upon the closing of the
			 * notification. A notification may be closed for a number of reasons,
			 * for example it may be closed by the user or simply time-out. All
			 * available return values are listed in ClosedReason.
			 *
			 * Prototype:
			 * void on_my_signal_closed(ClosedReason reason);
			 *
			 * @note If you are interested only in the case where a user has
			 *       actively closed the notification by clicking the window body
			 *       (and <emph>not</emph> the window's close button), you could
			 *       also use add_action() with "default" as the identifier.
			 */
			sigc::signal<void,ClosedReason>& signal_closed();

			private:
			// NOTE: We've tried using Glib::SignalProxy here, but unfortunately
			//       couldn't get to work for a variety of reasons, including, but not
			//       limited to:
			//       The constructor for SignalProxy expects a class derived from
			//       Glib::ObjectBase. This leads to problems, because since we are
			//       not really wrapping any Glib or Gtk object we don't know how
			//       to properly initialize, which leads to warnings and errors.
			//       also Glib::ObjectBase is non copyable, so we need a bunch of
			//       create() methods, and return a Glib::RefPtr<>. This then tries
			//       to ref() and unref() our non-existant object. Also if we try to
			//       use Glib::SignalProxy by passing a NULL pointer to its
			//       constructor it gets real unhappy fast.
			//       So we provide our own function which is somewhat syntax
			//       compatible with Glib::SignalProxy1 in usage.
			//Glib::SignalProxy1<void,int> signal_closed();
			sigc::signal<void,ClosedReason> signal_closed_;

			/* Implementation */
		private:
			/* Keep constructors private */
			Notification();
			Notification(const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());
			Notification(const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());
			Notification(Gtk::Widget& widget, const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());
			Notification(Gtk::Widget& widget, const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());
			Notification(const Glib::RefPtr<Gtk::StatusIcon> statusicon, const Glib::ustring summary, const Glib::ustring body = "", const Gtk::StockID icon = Gtk::StockID());
			Notification(const Glib::RefPtr<Gtk::StatusIcon> statusicon, const Glib::ustring summary, const Glib::ustring body = "", const Glib::RefPtr<Gdk::Pixbuf> icon = Glib::RefPtr<Gdk::Pixbuf>());
			/* Non-copyable, like Glib::ObjectBase */
			Notification(const Notification&);
			Notification& operator=(const Notification&);

			static boost::mutex         n_instances_mutex;
			static int                  n_instances;
			static Glib::ustring        application_name;
			static void notify_action_free_function(gpointer user_data);
			static void notify_action_callback(NotifyNotification *notification, gchar *action, gpointer user_data);
			gulong signal_closed_callback_id;
			static void signal_closed_callback(NotifyNotification* notify_notification, Notification* notification);
			void check_libnotify();
			void initialize_notification();
			void reinitialize();
			void ensure_notify_notification();
			guint next_action_id;
			NotifyNotification* notify_notification;
			GtkWidget* attached_widget;
			Glib::RefPtr<Gtk::StatusIcon> attached_statusicon;
			Glib::ustring summary;
			Glib::ustring body;
			Glib::ustring category;
			NotifyUrgency urgency;
			Gtk::StockID icon;
			Glib::RefPtr<Gdk::Pixbuf> icon_pixbuf;
	};
} // namespace Gtk

#endif // NOTIFICATION_H
