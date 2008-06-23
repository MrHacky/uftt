#include "Types.h"


#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_ipx.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/ipx_conn.h"
#include "SharedData.h"

#include <boost/bind.hpp>


using namespace std;

using boost::asio::ipx;


#define BUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;

int main( int argc, char **argv )
{



/*
	try {
	char dwrite[] = "writing string\n";
	asio_file_stream fs(service);
	cout << "opening file\n";
	fs.open("test.dat", fs.create|fs.out);
	cout << "starting async write\n";
	boost::asio::async_write(fs, boost::asio::buffer(dwrite, strlen(dwrite)), boost::bind(&write_done, &fs, boost::asio::placeholders::error));

	terminating = false;

	cout << "starting service\n";
	service.run();
	cout << "done\n";

	} catch(std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
*/
	return 0;

	//NetworkThread thrd1obj;

	QTMain gui(argc, argv);

	//gui.BindEvents(&thrd1obj);

	//boost::thread thrd1(thrd1obj);

	int ret = gui.run();

	terminating = true;

	//thrd1.join();

	return ret;
}
