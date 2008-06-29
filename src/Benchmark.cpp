#include "Types.h"


#include "net-asio/asio_ipx.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/ipx_conn.h"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::microsec_clock;

ptime starttime;
ptime stoptime;

using namespace std;

using boost::asio::ipx;

uint32 TESTBUFSIZE = (1024*1024*16);
std::vector<uint8*> testbuffers;
std::vector<boost::asio::mutable_buffer> bufseq;

void memcopy_test() {
	ptime start(microsec_clock::local_time());

	// copy first half of testbuffers to second half
	uint32 chunksize = 512;//TESTBUFSIZE; // must cleanly divide TESTBUFSIZE
	uint32 numbuffer = testbuffers.size() / 2;

	for (uint i = 0; i < numbuffer; ++i) {
		uint8* frombuf = testbuffers[i];
		uint8* tobuf   = testbuffers[i + numbuffer];
		for (uint j = 0; j < TESTBUFSIZE; j += chunksize, frombuf += chunksize, tobuf += chunksize)
			memcpy(tobuf, frombuf, chunksize);
	}
	ptime stop(microsec_clock::local_time());
	cout << "memcpy of " << 16*numbuffer << "MB in " << chunksize << "b chunks, ";
	cout << "took " << (stop-start) << endl;

}

char sendstr[] = "hello world!";
char recvstr[] = "12334567890!";

void write_done(const boost::system::error_code& e, size_t len) {
	if (e) {
		cout << "write failed: " << e.message() << endl;
	} else {
		stoptime = ptime(microsec_clock::local_time());
		cout << "written: " << len << endl;
		cout << "took " << (stoptime-starttime) << endl;
	}
}

void read_done(const boost::system::error_code& e, size_t len) {
	if (e) {
		cout << "read failed: " << e.message() << endl;
	} else {
		stoptime = ptime(microsec_clock::local_time());
		cout << "read:" << len << " : " << recvstr << endl;
		cout << "took " << (stoptime-starttime) << endl;
	}
}

template<typename ConnType>
void connection_ready(const boost::system::error_code& e, ConnType* conn, bool send)
{
	if (e) {
		cout << "connection failed: " << e.message() << endl;
	} else {
		cout << "connected!" << endl;
		starttime = ptime(microsec_clock::local_time());
		if (send) {
			boost::asio::async_write(*conn, bufseq, boost::bind(&write_done, _1, _2));
		} else {
			boost::asio::async_read(*conn, bufseq, boost::bind(&read_done, _1, _2));
		}
		//conn->async_write_some(boost::asio::buffer(sendstr), boost::bind(&write_done, _1, _2));
		//conn->async_read_some(boost::asio::buffer(recvstr), boost::bind(&read_done, _1, _2));
	}
}

int imain( int argc, char **argv )
{
	TESTBUFSIZE = 1024*1024*1*2;
	uint32 numbuffers = 32*2*2*2;
	cout << "initializing buffers" << flush;
	try {
		for (uint i = 0; i < numbuffers; ++i) {
			uint8* buf = new uint8[TESTBUFSIZE];
			for (uint j = 0; j < TESTBUFSIZE; ++j) {
				buf[j] = j & 0xff;
			}
			testbuffers.push_back(buf);
			bufseq.push_back(boost::asio::mutable_buffer(buf, TESTBUFSIZE));
			cout << '.' << flush;
		}
	} catch (...) {
		cout << "error at size: " << testbuffers.size() << endl;
	}

	cout << "done" << endl;

	memcopy_test();
	memcopy_test();
	memcopy_test();

	boost::asio::io_service service;
	boost::asio::io_service::work work(service);
	services::diskio_service diskio(service);
/*
	boost::asio::ip::tcp::acceptor tcp_accept(service);
	boost::asio::ip::tcp::socket tcp_sock(service);

	try {
		tcp_accept.open(boost::asio::ip::tcp::v4());
		tcp_accept.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), 2345));
		tcp_accept.listen(16);
		tcp_accept.async_accept(tcp_sock,
			boost::bind(&connection_ready<boost::asio::ip::tcp::socket>, _1, &tcp_sock, true));
		cout << "accept started" << endl;
	} catch (...) {
		tcp_sock.open(boost::asio::ip::tcp::v4());
		tcp_sock.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 2345),
			boost::bind(&connection_ready<boost::asio::ip::tcp::socket>, _1, &tcp_sock, false));
		cout << "connect started" << endl;
	}

	cout << "starting service" << endl;
	service.run();
	cout << "exiting" << endl;
*/
	ipx_acceptor acceptor(service);
	ipx_conn ipxconn(service);
	try {
		acceptor.open();
		acceptor.bind(ipx::endpoint(ipx::address::local(), 2345));
		acceptor.listen();
		acceptor.async_accept(ipxconn,
			boost::bind(&connection_ready<ipx_conn>, _1, &ipxconn, true));
		cout << "accept started" << endl;
	} catch (...) {
		boost::shared_ptr<boost::asio::ipx::socket> ipxsock(new boost::asio::ipx::socket(service));;
		ipxsock->open();
		ipxsock->set_option(boost::asio::ipx::socket::broadcast(true));
		ipxconn.async_connect(ipxsock, ipx::endpoint(ipx::address::broadcast(), 2345),
			boost::bind(&connection_ready<ipx_conn>, _1, &ipxconn, false));
		cout << "connect started" << endl;
	}

	cout << "starting service" << endl;
	service.run();
	cout << "exiting" << endl;

	return 0;
}

int main( int argc, char **argv ) {
	try {
		return imain(argc, argv);
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}
