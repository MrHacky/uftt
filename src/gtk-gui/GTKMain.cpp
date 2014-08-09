#include "GTKMain.h"
#include "GTKImpl.h"
#include "Notification.h"
#include <functional>

using namespace std;

// implementation class (PIMPL idiom)
class GTKImpl {
	public:
		GTKImpl( int& argc, char **argv, UFTTSettingsRef _settings)
		: settings(_settings)
		{
			#ifdef SUPPORT_GTK24
			if(!Glib::thread_supported())
				Glib::thread_init();
			#endif

			// This may throw, but that is indicative of a serious bug in either
			// guftt OR Notification (i.e. must not happen), so we shouldn't catch
			// but rather fix the cause of the crash.
			Gtk::Notification::set_application_name("UFTT");

			// kit and wnd should only be initialized after glib::thread_init()
			kit = shared_ptr<Gtk::Main>(new Gtk::Main(argc, argv));
			wnd = shared_ptr<UFTTWindow>(new UFTTWindow(settings));
		};

		UFTTSettingsRef settings;
		shared_ptr<Gtk::Main> kit;
		shared_ptr<UFTTWindow> wnd;
	private:
		GTKImpl(const GTKImpl&);
};

GTKMain::GTKMain(int argc, char **argv, UFTTSettingsRef settings)
: impl(shared_ptr<GTKImpl>(new GTKImpl(argc, argv, settings)))
{
}

GTKMain::~GTKMain() {
}

void GTKMain::bindEvents(UFTTCore* core) {
	impl->wnd->set_backend(core);
}

int GTKMain::run() {
	impl->wnd->pre_show();
	if(!impl->settings->start_in_tray) {
		impl->wnd->show_all();
		impl->wnd->present();
	}
	impl->wnd->post_show();
	impl->kit->run();
	impl->wnd->hide();
	return 0;
}

const shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	return shared_ptr<GTKMain>(new GTKMain(argc, argv, settings));
}

#ifndef DEBUG
	#ifdef WIN32
		// This is an ugly hack, but guftt won't link otherwise.
		// It will complain about missing winmain@16.
		// I'm not sure how/why this works with Qt.
		// We could perhaps put this in main.cpp, but since right
		// now it's a GTK specific isue I'm putting it here.
		#include <windows.h>
		extern int main(int argc, char **argv);
		int APIENTRY WinMain(HINSTANCE hInstance,
							 HINSTANCE hPrevInstance,
							 LPSTR     lpCmdLine,
							 int       nCmdShow)
		{
			return main(nCmdShow, &lpCmdLine);
		}
	#endif
#endif
