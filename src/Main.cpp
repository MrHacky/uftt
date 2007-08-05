#include "Types.h"

#include "qt-gui/QTMain.h"
#include "network/NetworkThread.h"
#include "SharedData.h"

int main( int argc, char **argv )
{
	terminating = false;

	boost::thread thrd1((NetworkThread()));

	// main(this) thread will handle the GUI, in addition to any threads QT might create internally
	bool ret = ShowQTGui(argc, argv);

	terminating = true;

	thrd1.join();

	return ret;
}
