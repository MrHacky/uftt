#ifndef UFTT_GUI_H
	#define UFTT_GUI_H

	#include "UFTTCore.h"
	#include "UFTTSettings.h"
	#include <boost/noncopyable.hpp>

	class UFTTGui : boost::noncopyable {
		public:
			/**
			 * Instantiate an instance of a GUI. argc and argv can be passed to the
			 * GUI's main-loop initialization (e.g. QApplication or Gtk::Main).
			 * An instance of UFTTSettings is passed to allow the gui to properly
			 * initialize itself to the state stored in the settings. E.g. window
			 * size/position and various other settings.
			 * @param argc is the number of commandline arguments passed in argv.
			 * @param argv is an array of strings (argc many) which are the commandline arguments.
			 * @param settings are the settings the gui should use to initialize itself.
			 *         (E.g. window position, the state of radio-buttons, etc.)
			 */
			static const boost::shared_ptr<UFTTGui> makeGui(int argc, char** argv, UFTTSettingsRef settings);

			/**
			 * After the GUI has been created using makeGui you need to call bindEvents()
			 * to set the UFTTCore which the gui should control. By binding the Core to
			 * the GUI after the gui is been instantiated we give the GUI time to do
			 * things such as redirect std::err and std::out to a graphical debug log,
			 * which is important since we don't want to miss (debug) messages from the
			 * core.
			 * @param core is a reference to the core that the gui should control.
			 */
			virtual void bindEvents(UFTTCore* core) = 0;

			/**
			 * After the GUI had been created and a core has been bound to the GUI
			 * using bindEvents() you can call run(). Run executes the GUI thread
			 * until the user wants to quit.
			 */
			virtual int run() = 0;
	};

#endif // UFTT_GUI_H
