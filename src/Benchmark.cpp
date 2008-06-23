#include "Types.h"


#include "net-asio/asio_ipx.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/ipx_conn.h"

#include <boost/bind.hpp>

using namespace std;

using boost::asio::ipx;

services::diskio_service* gdiskio;
char gbuf[10];

void open_read_some(const boost::system::error_code& e, size_t len, services::diskio_filetype* file)
{
	cout << "open read some done\n";
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

char sendstring[] = "sending string";
char recvstring[] = "12345678902343";

void write_some_done(const boost::system::error_code& e, size_t sent, ipx_conn* sock)
{
	cout << "write some done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
	}
}

void read_some_done(const boost::system::error_code& e, size_t sent, ipx_conn* sock)
{
	cout << "read some done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		cout << "received: " << recvstring << '\n';
	}
}

void accept_done(const boost::system::error_code& e, ipx_conn* conn)
{
	cout << "accept done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		conn->async_read_some(boost::asio::buffer(recvstring),
			boost::bind(&read_some_done, _1, _2, conn));
	}
}

void connect_done(const boost::system::error_code& e, ipx_conn* conn)
{
	cout << "connect done\n";
	if (e) {
		cout << "failed: " << e.message() << '\n';
	} else {
		conn->async_write_some(boost::asio::buffer(sendstring),
			boost::bind(&write_some_done, _1, _2, conn));
	}
}


#define TESTBUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;

void connection_ready(const boost::system::error_code& e, ipx_conn* conn)
{
	if (e) {
		cout << "connection failed: " << e.message() << endl;
	} else {
		cout << "connected!" << endl;
	}
}

int imain( int argc, char **argv )
{
	cout << "initializing buffers" << flush;
	try {
		for (int i = 0; i < 32; ++i) {
			uint8* buf = new uint8[TESTBUFSIZE];
			for (int j = 0; j < TESTBUFSIZE; ++j) {
				buf[j] = j & 0xff;
			}
			testbuffers.push_back(buf);
			cout << '.' << flush;
		}
	} catch (...) {
		cout << "error at size: " << testbuffers.size() << endl;
	}

	cout << "done" << endl;

	boost::asio::io_service service;
	boost::asio::io_service::work work(service);
	services::diskio_service diskio(service);

	ipx_acceptor acceptor(service);
	ipx_conn ipxconn(service);
	try {
		acceptor.open();
		acceptor.bind(ipx::endpoint(ipx::address::local(), 2345));
		acceptor.listen();
		acceptor.async_accept(ipxconn,
			boost::bind(&connection_ready, _1, &ipxconn));
		cout << "accept started" << endl;
	} catch (...) {
		boost::shared_ptr<boost::asio::ipx::socket> ipxsock(new boost::asio::ipx::socket(service));;
		ipxsock->open();
		ipxsock->set_option(boost::asio::ipx::socket::broadcast(true));
		ipxconn.async_connect(ipxsock, ipx::endpoint(ipx::address::broadcast(), 2345),
			boost::bind(&connection_ready, _1, &ipxconn));
		cout << "connect started" << endl;
	}

	cout << "starting service" << endl;
	service.run();
	cout << "exiting" << endl;

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
}

int main( int argc, char **argv ) {
	try {
		return imain(argc, argv);
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}