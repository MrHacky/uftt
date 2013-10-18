#include "ShowURI.h"
#include "../util/StrFormat.h"
#include <iostream>
#include <gtkmm.h>
#include <glibmm/spawn.h>

using namespace std;

void child_setup() {}

namespace Gtk {
	void show_uri(Glib::RefPtr<Gdk::Screen> screen,
	              Glib::ustring uri,
	              guint32 timestamp
	             )
	{
		if(gtk_show_uri(screen?screen->gobj():NULL, uri.c_str(), timestamp, NULL))
			return;

		try {
			Glib::spawn_command_line_async(STRFORMAT("exo-open \"%s\"", uri));
			return;
		} catch(Glib::SpawnError e) {}

		try {
			Glib::spawn_command_line_async(STRFORMAT("xdg-open \"%s\"", uri));
			return;
		} catch(Glib::SpawnError e) {}

		throw Glib::SpawnError(Glib::SpawnError::INVAL, STRFORMAT("Could not open URL \"%s\"", uri));
	}
}
