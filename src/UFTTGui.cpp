#include "UFTTGui.h"
//#define ENABLE_GTK_GUI // FIXME: This should be set from CMake
#ifdef ENABLE_GTK_GUI
	#include "gtk-gui/GTKMain.h"
#else
	#include "qt-gui/QTMain.h"
#endif

const boost::shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	#ifdef ENABLE_GTK_GUI
		return boost::shared_ptr<UFTTGui>(new GTKMain(argc, argv, settings));
	#else // Default to QT Gui
		return boost::shared_ptr<UFTTGui>(new QTMain(argc, argv, settings));
	#endif
}
