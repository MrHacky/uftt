#include "QTMain.h"

#include <QApplication>

#include <boost/bind.hpp>

#include "MainWindow.h"

using namespace std;

// implementation class (PIMPL idiom)
class QTImpl {
	public:
		QApplication app;
		MainWindow wnd;

		QTImpl(int argc, char **argv, UFTTSettingsRef settings)
			: app(argc, argv), wnd(settings)
		{
			QObject::connect(&app, SIGNAL(focusChanged(QWidget*, QWidget*)), &wnd, SLOT(onFocusChanged(QWidget*, QWidget*)));
		};

	private:
		QTImpl(const QTImpl&);
};

QTMain::QTMain(int argc, char **argv, UFTTSettingsRef settings)
{
	impl = new QTImpl(argc, argv, settings);
}

QTMain::~QTMain()
{
	delete impl;
}

void QTMain::bindEvents(UFTTCore* core)
{
	impl->wnd.SetBackend(core);
}

int QTMain::run()
{
	impl->wnd.pre_show();
	impl->wnd.show();
	impl->wnd.post_show();
	return impl->app.exec();
}
