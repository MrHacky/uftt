#include "GTKMain.h"
#include "GTKImpl.h"
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;

// implementation class (PIMPL idiom)
class GTKImpl {
	public:
		GTKImpl( int& argc, char **argv, UFTTSettingsRef _settings)
		: settings(_settings)
		{
			if(!Glib::thread_supported())
				Glib::thread_init();
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

GTKMain::~GTKMain()
{
}

void GTKMain::bindEvents(UFTTCoreRef core)
{
//	impl->wnd.SetBackend(t);
}

int GTKMain::run()
{
	impl->wnd->show();
	Gtk::Main::run(*impl->wnd); // returns when wnd is hidden (use Gtk::Main::run(void) & Gtk::Main::quit() when implementing tray-icon)
	impl->wnd->hide();
	return 0;
}
