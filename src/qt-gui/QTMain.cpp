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

		QTImpl( int argc, char **argv )
			: app(argc,argv) { };

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
	/*
	nwobj->cbAddServer = boost::bind(
		QtBooster(&impl->wnd, SLOT(AddNewServer()))
	);
	nwobj->cbAddShare = boost::bind(
		QtBooster(&impl->wnd, SLOT(AddNewShare(std::string, SHA1))),
		_1, _2
	);
	nwobj->cbNewTreeInfo = boost::bind(
		QtBooster(&impl->wnd, SLOT(NewTreeInfo(JobRequestRef))),
		_1
	);
	*/
}

int QTMain::run()
{
	impl->wnd.show();
	return impl->app.exec();
}
