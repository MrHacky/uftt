#include "../Types.h"

#include "../net-asio/asio_ipx.h"
#include "../net-asio/asio_file_stream.h"
#include "../net-asio/ipx_conn.h"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

typedef boost::asio::ip::udp proto;
//typedef boost::asio::ipx proto;

using namespace std;

uint64 rnum(0);
uint64 snum(0);

void timertick(boost::asio::deadline_timer* timer, const  boost::system::error_code& e =  boost::system::error_code())
{
	if (e) return;
	cout << "tick: " << snum << " - " << rnum << "\n";
	rnum = 0;
	snum = 0;
	timer->expires_from_now(boost::posix_time::milliseconds(1000));
	timer->async_wait(boost::bind(&timertick, timer, _1));
}

proto::endpoint rep;
proto::endpoint sep;
uint8 rbuf[1406];
uint8 sbuf[1406];

void recvpacket(proto::socket* sock, const boost::system::error_code& e =  boost::system::error_code(), size_t len = 0)
{
	if (e) return;
	rnum += len;

	sock->async_receive_from(boost::asio::buffer(rbuf), rep,
		boost::bind(&recvpacket, sock, _1, _2));
}

void sendpacket(proto::socket* sock, const boost::system::error_code& e =  boost::system::error_code(), size_t len = 0)
{
	if (e) return;
	snum += len;
/*
	sock->send_to(boost::asio::buffer(sbuf), sep);
	snum += 1406;
	sock->send_to(boost::asio::buffer(sbuf), sep);
	snum += 1406;
	sock->send_to(boost::asio::buffer(sbuf), sep);
	snum += 1406;
	sock->send_to(boost::asio::buffer(sbuf), sep);
	snum += 1406;
	sock->send_to(boost::asio::buffer(sbuf), sep);
	snum += 1406;
*/
	sock->async_send_to(boost::asio::buffer(sbuf), sep,
		boost::bind(&sendpacket, sock, _1, _2));
}

int imain( int argc, char **argv )
{
	boost::asio::io_service service;
	boost::asio::io_service::work work(service);
	services::diskio_service diskio(service);

	proto::socket socket(service);
	boost::asio::deadline_timer timer(service);
	timertick(&timer);

	socket.open(sep.protocol());
	//socket.bind(proto::endpoint(proto::address::local(), 33433));
	//socket.bind(proto::endpoint(boost::asio::ip::address_v4(), 33433));
	socket.bind(proto::endpoint(boost::asio::ip::address::from_string("192.168.0.42"), 33433));
	socket.set_option(proto::socket::broadcast(true));
	//sep = proto::endpoint(proto::address::broadcast(), 33433);//socket.local_endpoint();
	//sep = proto::endpoint(boost::asio::ip::address_v4::broadcast(), 33433);//socket.local_endpoint();
	//sep = proto::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 33433);
	sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.255"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("255.255.255.255"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.32"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.42"), 33433);

	//recvpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);

	boost::thread srthread(boost::bind(&boost::asio::io_service::run, &service));

	/*
	string str;
	cin >> str;
	*/
	return 0;
}

int main( int argc, char **argv ) {

}
