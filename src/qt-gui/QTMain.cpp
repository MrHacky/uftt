#include "QTMain.h"

#include <QApplication>

#include "MainWindow.h"

using namespace std;

int ShowQTGui( int argc, char **argv )
{
	QApplication app(argc, argv);

	MainWindow mainwindow;
	mainwindow.show();
	
	return app.exec();
}
