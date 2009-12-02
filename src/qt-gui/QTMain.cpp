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

		QTImpl( int& argc, char **argv, UFTTSettings& settings)
			: app(argc, argv), wnd(settings)
		{
			QObject::connect(&app, SIGNAL(focusChanged(QWidget*, QWidget*)), &wnd, SLOT(onFocusChanged(QWidget*, QWidget*)));
		};

	private:
		QTImpl(const QTImpl&);
};

QTMain::QTMain( int& argc, char **argv, UFTTSettings* settings)
{
	impl = new QTImpl(argc, argv, *settings);
}

QTMain::~QTMain()
{
	delete impl;
}

void QTMain::bindEvents(UFTTCore* t)
{
	impl->wnd.SetBackend(t);
}

int QTMain::run()
{
	impl->wnd.pre_show();
	impl->wnd.show();
	impl->wnd.post_show();
	return impl->app.exec();
}
