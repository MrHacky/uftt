#include "Types.h"

#include "qt-gui/QTMain.h"
#include "network/NetworkThread.h"

int main( int argc, char **argv )
{
	boost::thread thrd1((NetworkThread()));

	// main(this) thread will handle the GUI, in addition to any threads QT might create internally
	bool ret = ShowQTGui(argc, argv);

	thrd1.join();

	return ret;
}
