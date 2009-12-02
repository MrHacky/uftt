#ifndef GMPMPC_H
	#define GMPMPC_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include "dispatcher_marshaller.h"
	#include <iostream>
	#include <gtkmm/main.h>
	#include <gtkmm/window.h>

	class UFTTWindow : public Gtk::Window {
		public:
			UFTTWindow(UFTTCore& _core, UFTTSettings& _settings);

		private:
				UFTTCore& core;
				UFTTSettings& settings;
			/* Variables */
//			DispatcherMarshaller dispatcher; // Execute a function in the gui thread

			/* Helpers */

			/* Containers (widgets not referenced by code) */

			/* Widgets (referenced by code) */

			/* Functions */
	};

#endif
