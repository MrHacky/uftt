#include "QTMain.h"

#include <QApplication>

#include <boost/bind.hpp>

#include "MainWindow.h"

using namespace std;

struct QTImpl {
	QApplication* app;
	MainWindow* wnd;
};

QTMain::QTMain( int argc, char **argv )
{
	impl = new QTImpl();
	((QTImpl*)impl)->app = new QApplication(argc, argv);
	((QTImpl*)impl)->wnd = new MainWindow();
	QObject::connect(((QTImpl*)impl)->wnd->action_Quit, SIGNAL(activated()),
	                 ((QTImpl*)impl)->app, SLOT(quit()));
}

QTMain::~QTMain()
{
	delete ((QTImpl*)impl)->wnd;
	delete ((QTImpl*)impl)->app;
	delete ((QTImpl*)impl);
}

void QTMain::BindEvents(NetworkThread* nwobj)
{
	nwobj->cbAddServer = boost::bind(
		&MainWindow::emitAddNewServer,
		((QTImpl*)impl)->wnd
	);
	//nwobj->cbAddServer = QTSignalFunction();
	nwobj->cbAddShare = boost::bind(
		&MainWindow::emitAddNewShare,
		((QTImpl*)impl)->wnd,
		_1, _2
	);
}

int QTMain::run()
{
	((QTImpl*)impl)->wnd->show();
	((QTImpl*)impl)->app->exec();
}
