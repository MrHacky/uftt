#include "Types.h"

#include "qt-gui/QTMain.h"
#include "network/NetworkThread.h"
#include "SharedData.h"

int main( int argc, char **argv )
{
	terminating = false;

	NetworkThread thrd1obj;

	QTMain gui(argc, argv);

	gui.BindEvents(&thrd1obj);

	boost::thread thrd1(thrd1obj);

	int ret = gui.run();

	terminating = true;

	thrd1.join();

	return ret;
}
