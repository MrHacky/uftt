#include <QApplication>

#include "MainWindow.h"
#include "../Types.h"
#include "../network/CrossPlatform.h"
#include "../files/FileInfo.h"

using namespace std;

int main( int argc, char **argv )
{
	QApplication app(argc, argv);

	MainWindow mainwindow;
	mainwindow.show();
	
	return app.exec();
}
