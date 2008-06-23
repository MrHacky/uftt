#include "Types.h"

#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/asio_ipx.h"
#include "SharedData.h"

int main( int argc, char **argv )
{
	boost::asio::io_service service;
	
	boost::asio::ipx::socket ipxsock1(service);
	boost::asio::ipx::socket ipxsock2(service);

	ipxsock1.open();
	ipxsock2.open();

	ipxsock1.bind(boost::asio::ipx::endpoint(boost::asio::ipx::address::local(), 1234));
	ipxsock2.set_option(boost::asio::ipx::socket::broadcast(true));

	char send[] = "sending string";
	char recv[] = "12345678902343";

	ipxsock2.send_to(boost::asio::buffer(send, strlen(send)), boost::asio::ipx::endpoint(boost::asio::ipx::address::broadcast_local(), 1234));

	boost::asio::ipx::endpoint ep;

	ipxsock1.receive_from(boost::asio::buffer(recv, strlen(recv)),ep);


	asio_file_stream fs(service);
	terminating = false;

	//NetworkThread thrd1obj;

	QTMain gui(argc, argv);

	//gui.BindEvents(&thrd1obj);

	//boost::thread thrd1(thrd1obj);

	int ret = gui.run();

	terminating = true;

	//thrd1.join();

	return ret;
}
