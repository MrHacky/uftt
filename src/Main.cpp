#include "Types.h"


#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/asio_ipx.h"
#include "SharedData.h"

#include <boost/bind.hpp>


using namespace std;

using boost::asio::ipx;

services::diskio_service* gdiskio;
char gbuf[10];

void open_read_some(const boost::system::error_code& e, size_t len, services::diskio_filetype* file)
{
	cout << "read some done\n";
	if (!e) {
		cout << len << ':' << std::string(gbuf, gbuf+len) << '\n';
		file->async_read_some(boost::asio::buffer(gbuf), 
			boost::bind(&open_read_some, _1, _2, file));
	} else {
		cout << "error: " << e << " " << e.message() << '\n';
	}
}

void read_file_some_done(const boost::system::error_code& e, services::diskio_filetype* file)
{
	cout << "reopen done\n";
	if (!e) {
		file->async_read_some(boost::asio::buffer(gbuf), 
			boost::bind(&open_read_some, _1, _2, file));
	} else {
		cout << "error: " << e << " " << e.message() << '\n';
	}
}


void write_done(services::diskio_filetype* file, const boost::system::error_code& e)
{
	cout << "write done\n";
	if (!e) {
		file->close();
		file->close();
		cout << "closed file\n";
		gdiskio->async_open_file(string("d:\\temp\\tdir\\test.dat"), services::diskio_filetype::in, *file,
			boost::bind(&read_file_some_done, boost::asio::placeholders::error, file));

	} else {
		cout << "error: " << e << " " << e.message() << '\n';
	}
}

void open_done(const boost::system::error_code& e, services::diskio_filetype* file)
{
	cout << "open done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		char* dwrite = "writing string\n";
		boost::asio::async_write(*file, boost::asio::buffer(dwrite, strlen(dwrite)),
			boost::bind(&write_done, file, boost::asio::placeholders::error));
	}
}

void create_done(const boost::system::error_code& e, services::diskio_service* diskio, services::diskio_filetype* file)
{
	cout << "create done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		diskio->async_open_file(string("d:\\temp\\tdir\\test.dat"), services::diskio_filetype::out|services::diskio_filetype::create, *file,
			boost::bind(&open_done, boost::asio::placeholders::error, file));
	}
}

void receive_done(const boost::system::error_code& e, size_t sent, char* msg, ipx::endpoint* ep)
{
	cout << "receive done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		cout << sent << ':' << msg << '\n';
	}
	delete ep;
}

void send_done(const boost::system::error_code& e, size_t sent, char* msg, char* rbuf, ipx::socket* sock)
{
	cout << "send done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		cout << sent << ':' << msg << '\n';
		ipx::endpoint* ep = new ipx::endpoint();
		sock->async_receive_from(boost::asio::buffer(rbuf, strlen(rbuf)+1), *ep,
			boost::bind(&receive_done, _1, _2, rbuf, ep));

	}
}


int main( int argc, char **argv )
{
	boost::asio::io_service service;
	boost::asio::io_service::work work(service);

	services::diskio_service diskio(service);
	gdiskio = &diskio;
	services::diskio_filetype file(diskio);

	cout << "starting async create\n";
	diskio.async_create_directory(boost::filesystem::path("d:\\temp\\tdir"),
	boost::bind(&create_done, boost::asio::placeholders::error, &diskio, &file));

	ipx::socket ipxsock1(service);
	ipx::socket ipxsock2(service);

	boost::asio::ip::tcp::socket tsock(service);


	ipxsock1.open();
	ipxsock2.open();

	ipxsock1.bind(ipx::endpoint(ipx::address::local(), 1234));
	ipxsock2.set_option(ipx::socket::broadcast(true));

	char send[] = "sending string";
	char recv[] = "12345678902343";

	//tsock.write_some(boost::asio::buffer(send));
	ipxsock2.async_send_to(boost::asio::buffer(send), ipx::endpoint(ipx::address::broadcast_local(), 1234),
		boost::bind(&send_done, _1, _2, send, recv, &ipxsock1));

	cout << "starting service\n";
	service.run();
	cout << "done\n";

	return 0;


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
