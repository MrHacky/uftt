#ifndef SHOW_URL_H
	#define SHOW_URL_H
	#include <gdkmm/screen.h>
	#include <glibmm/refptr.h>
	#include <glibmm/ustring.h>

	namespace Gtk {
		/**
		 * show_uri is not (yet?) wrapped by Gtkmm, so we provide our own.
		 * Additionally this implementation of show_uri is more robust than
		 * the implementation present in Gtk+, as it does not fall back on
		 * epiphany but rather also tries 'exo-open' and 'xdg-open'.
		 * If all else fails show_uri throws a Glib::SpawnError.
		 * @param uri is the URI to be openened. Any URI supported by any of
		 *        gtk_show_uri, exo-open or xdg-open is supported.
		 */
		void show_uri(Glib::RefPtr<Gdk::Screen> screen,
		              Glib::ustring uri,
		              guint32 timestamp
		              );
	}

#endif // SHOW_URL_H
