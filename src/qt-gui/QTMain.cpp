#include "QTMain.h"

#include <QApplication>

#include <boost/bind.hpp>

#include "MainWindow.h"
#include "QtBooster.h"

using namespace std;

// implementation class (PIMPL idiom)
class QTImpl {
	public:
		QApplication app;
		MainWindow wnd;

		QTImpl( int& argc, char **argv, QTMain& owner_)
			: app(argc, argv), wnd(owner_)
		{

		};

	private:
		QTImpl(const QTImpl&);
};

QTMain::QTMain( int& argc, char **argv )
{
	impl = new QTImpl(argc, argv, *this);
}

QTMain::~QTMain()
{
	delete impl;
}

void QTMain::BindEvents(SimpleBackend* t)
{
	impl->wnd.SetBackend(t);
}

int QTMain::run()
{
	impl->wnd.show();
	return impl->app.exec();
}
