#include "../Types.h"

#if 0
#include "../net-asio/asio_ipx.h"
#include "../net-asio/asio_file_stream.h"
#include "../net-asio/ipx_conn.h"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../util/StrFormat.h"

//typedef boost::asio::ip::udp proto;
typedef boost::asio::ipx proto;

using namespace std;

uint64 rnum(0);
uint64 snum(0);

#include <boost/signal.hpp>

#if 0
class UDPHolePunchingSocket {
	private:
		Handler handler;
		QueryUDPHoleAdress* othis;

	public:
		static boost::asio::ip::address getAdress();

		template <typename TheHandler>
		static void getAdress(const TheHandler& handler)
		{
			QueryUDPHoleAdress<TheHandler>* qobj = new QueryUDPHoleAdress<TheHandler>()
		}
};
#endif

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

void listipxif()
{
#if 0
		SOCKET sd = WSASocket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX, 0, 0, 0);
		if (sd == SOCKET_ERROR) {
			cerr << "Failed to get a socket. Error " << WSAGetLastError() <<
				endl;
			return;
			//return rtype(result.begin(), result.end());
		}

		//INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		struct _SOCKET_ADDRESS_LIST {
	INT iAddressCount;
	SOCKET_ADDRESS Address[20];
} SOCKET_ADDRESS_LIST;



		if (WSAIoctl(sd, SIO_ADDRESS_LIST_QUERY, 0, 0, &SOCKET_ADDRESS_LIST,
				sizeof(SOCKET_ADDRESS_LIST), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
			cout << "Failed calling WSAIoctl: error " << WSAGetLastError() <<
					endl;
			return;// rtype(result.begin(), result.end());
		}

		int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
		//cout << "There are " << nNumInterfaces << " interfaces:" << endl;
		for (int i = 0; i < SOCKET_ADDRESS_LIST.iAddressCount; ++i) {
			boost::asio::ipx::endpoint nep;
			if ((int)nep.capacity() <= SOCKET_ADDRESS_LIST.Address[i].iSockaddrLength)
				memcpy(nep.data(), &SOCKET_ADDRESS_LIST.Address[i], SOCKET_ADDRESS_LIST.Address[i].iSockaddrLength);
			boost::asio::ipx::address naddr = nep.getAddress();
			cout << STRFORMAT("%02x:%02x:%02x:%02x", (int)naddr.network[0], (int)naddr.network[1], (int)naddr.network[2], (int)naddr.network[3]);
			cout << '-';
			cout << STRFORMAT("%02x:%02x:%02x:%02x:%02x:%02x", (int)naddr.node[0], (int)naddr.node[1], (int)naddr.node[2], (int)naddr.node[3], (int)naddr.node[4], (int)naddr.node[5]);
			cout << '\n';
			/*
			if (!(nFlags & IFF_LOOPBACK) && (nFlags & IFF_BROADCAST)) {
				for (int i = 0; i < 4; ++i)
					ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
				cout << "broadcast: " << naddr << '\n';
			}
			*/
		}

		//return 0;
#endif
}

int imain( int argc, char **argv )
{
	listipxif();
	boost::asio::io_service service;
	boost::asio::io_service::work work(service);
	services::diskio_service diskio(service);
	proto::socket socket(service);
	boost::asio::deadline_timer timer(service);
	timertick(&timer);
"dd:00:0e:00-00:00:a4:fc:dd:00";
"dd:00:0e:00-00:00:b4:fc:dd:00";
"dd:00:0e:00-00:00:c4:fc:dd:00";
"dd:00:0e:00-00:00:d4:fc:dd:00";
"dd:00:0e:00-00:00:06:00:12:34";

	boost::asio::ip::udp::endpoint stunaddr(boost::asio::ip::address::from_string("83.103.82.85"),3478);
	boost::asio::ip::udp::endpoint raddr;
	boost::asio::ip::udp::socket stunsock(service);
	stunsock.open(stunaddr.protocol());

	sbuf[ 0] = 0;
	sbuf[ 1] = 1;
	sbuf[ 2] = 0;
	sbuf[ 3] = 8;
	sbuf[13] = 11;
	sbuf[14] = 12;
	sbuf[20] = 0;
	sbuf[21] = 3;
	sbuf[22] = 0;
	sbuf[23] = 4;
	sbuf[24] = 0;
	sbuf[25] = 0;
	sbuf[26] = 0;
	sbuf[27] = 6;

//	char xx="\x00\x01\x00\x08         \x0B\x0C     \x00\x03\x00\x04\x00\x00\x00\x06";


	//stunsock.send_to(boost::asio::buffer("\x00\x01\x00\x08         \x0B\x0C     \x00\x03\x00\x04\x00\x00\x00\x06", 28), stunaddr);
	  stunsock.send_to(boost::asio::buffer("\x00\x01\x00\x00         \x0B\x0C     ", 20), stunaddr);

	stunsock.receive_from(boost::asio::buffer(rbuf), raddr);

	cout << "from: " << raddr << '\n';

	int len = (rbuf[2] << 8) | (rbuf[3] << 0);
	int idx = 20;

	while (idx-20 < len) {
		int type = 0;
		type |= (rbuf[idx++] << 8);
		type |= (rbuf[idx++] << 0);
		int tlen = 0;
		tlen |= (rbuf[idx++] << 8);
		tlen |= (rbuf[idx++] << 0);

		switch (type) {
			case 1: {
				cout << "MAPPED-ADDRESS: " << (int)rbuf[idx+4] << '.' << (int)rbuf[idx+5] << '.' << (int)rbuf[idx+6] << '.' << (int)rbuf[idx+7] << ':';
				cout << ((rbuf[idx+2] << 8) | (rbuf[idx+3] << 0));
				cout << '\n';
			}; break;
			case 4: {
				cout << "SOURCE-ADDRESS: " << (int)rbuf[idx+4] << '.' << (int)rbuf[idx+5] << '.' << (int)rbuf[idx+6] << '.' << (int)rbuf[idx+7] << ':';
				cout << ((rbuf[idx+2] << 8) | (rbuf[idx+3] << 0));
				cout << '\n';
			}; break;
			case 5: {
				cout << "CHANGED-ADDRESS: " << (int)rbuf[idx+4] << '.' << (int)rbuf[idx+5] << '.' << (int)rbuf[idx+6] << '.' << (int)rbuf[idx+7] << ':';
				cout << ((rbuf[idx+2] << 8) | (rbuf[idx+3] << 0));
				cout << '\n';
			}; break;
			default: cout << "type: " << type << '\n';
		}
		idx += tlen;
	}

	//boost::asio::ipx::address ipxaddr1 = {{0xdd, 0x00, 0x0e, 0x00}, {0x00, 0x00, 0xa4, 0xfd, 0xdd, 0x00}};
	//boost::asio::ipx::address ipxaddr1 = {{0xdd, 0x00, 0x0e, 0x00}, {0x00, 0x00, 0xb4, 0xfd, 0xdd, 0x00}};
	//boost::asio::ipx::address ipxaddr1 = {{0xdd, 0x00, 0x0e, 0x00}, {0x00, 0x00, 0xc4, 0xfd, 0xdd, 0x00}};
	//boost::asio::ipx::address ipxaddr1 = {{0xdd, 0x00, 0x0e, 0x00}, {0x00, 0x00, 0xd4, 0xfd, 0xdd, 0x00}};
	  boost::asio::ipx::address ipxaddr1 = {{0xdd, 0x00, 0x0e, 0x00}, {0x00, 0x00, 0x06, 0x00, 0x12, 0x34}};

	sep = proto::endpoint(ipxaddr1, 33433);

//	ipxaddr = ;

	//sep = proto::endpoint(proto::address::broadcast(), 33433);//socket.local_endpoint();
	//sep = proto::endpoint(boost::asio::ip::address_v4::broadcast(), 33433);//socket.local_endpoint();
	//sep = proto::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.255"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("255.255.255.255"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.24"), 33433);
	//sep = proto::endpoint(boost::asio::ip::address::from_string("192.168.0.42"), 33433);

	socket.open(sep.protocol());
	//socket.bind(proto::endpoint(ipxaddr1, 33433));
	//socket.bind(proto::endpoint(boost::asio::ip::address::from_string("192.168.0.42"), 33433));
	//socket.bind(proto::endpoint(proto::address::local(), 33433));
	//socket.bind(proto::endpoint(boost::asio::ip::address_v4(), 33433));
	socket.set_option(proto::socket::broadcast(true));

	recvpacket(&socket);
	recvpacket(&socket);
	recvpacket(&socket);
	recvpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);
	sendpacket(&socket);

	boost::thread srthread(boost::bind(&boost::asio::io_service::run, &service));

	string str;
	cin >> str;
	return 0;
}

#endif
int main( int argc, char **argv ) {
#if 0
	try {
		return imain(argc, argv);
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
#endif
}
