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

		QTImpl( int argc, char **argv )
			: app(argc,argv)
		{
			QObject::connect(wnd.action_Quit, SIGNAL(triggered()),
			                 &app           , SLOT(quit()));
		}

	private:
		QTImpl(const QTImpl&);
};

QTMain::QTMain( int argc, char **argv )
{
	impl = new QTImpl(argc, argv);
}

QTMain::~QTMain()
{
	delete impl;
}

void QTMain::BindEvents(NetworkThread* nwobj)
{
	nwobj->cbAddServer = boost::bind(
		&MainWindow::emitAddNewServer,
		&impl->wnd
	);
	//nwobj->cbAddServer = QTSignalFunction();
	nwobj->cbAddShare = boost::bind(
		&MainWindow::emitAddNewShare,
		&impl->wnd,
		_1, _2
	);
	nwobj->cbNewTreeInfo = boost::bind(
		&MainWindow::emitNewTreeInfo,
		&impl->wnd,
		_1
	);
}

int QTMain::run()
{
	impl->wnd.show();
	return impl->app.exec();
}
