#if 0
#include <windows.h>

int APIENTRY synesisWinMain(HINSTANCE,
                            HINSTANCE,
                            LPSTR,
                            int)
{
    int iRet;
/*
    iRet = WinMain( GetModuleHandle(NULL),
                    NULL,
                    GetAdjustedCommandLine(),
                    GetWindowShowState());

    trace_leaks();
*/
    return iRet;
}
#endif

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "../util/StrFormat.h"


char udp_recv_buf[10];
boost::asio::ip::udp::endpoint udp_recv_addr;

void start_receive(boost::asio::ip::udp::socket& s);

void do_receive(boost::asio::ip::udp::socket& s, const boost::system::error_code& e, size_t len ) {
	std::cout << "source: " << udp_recv_addr << '\n';

	start_receive(s);
}

void start_receive(boost::asio::ip::udp::socket& s) {
	s.async_receive_from(boost::asio::buffer(udp_recv_buf), udp_recv_addr,
		boost::bind(&do_receive, boost::ref(s), _1, _2)
		);
}

int main( int argc, char **argv ) {
#if defined(WIN32) && defined(SIO_ADDRESS_LIST_QUERY)
	{
		int n_v6_interfaces = 0;

		LPSOCKET_ADDRESS_LIST v6info;
		SOCKET_ADDRESS* addrs;
		DWORD buflen = sizeof (SOCKET_ADDRESS_LIST) + (63 * sizeof (SOCKET_ADDRESS ));
		char *buffer = new char[buflen];

		v6info = reinterpret_cast<LPSOCKET_ADDRESS_LIST> (buffer);
		addrs = reinterpret_cast<SOCKET_ADDRESS *> (buffer + sizeof (SOCKET_ADDRESS_LIST));

		// Get an (overlapped) DGRAM socket to test with.
		// If it fails only return IPv4 interfaces.
		SOCKET sock = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET)
		 {
		   delete [] buffer;
//		   return -1;
		 }

		DWORD bytes;

		int status = WSAIoctl(sock,
						 SIO_ADDRESS_LIST_QUERY,
						 0,
						 0,
						 v6info,
						 buflen,
						 &bytes,
						 0,
						 0);
		closesocket (sock);
		if (status == SOCKET_ERROR)
		{
		   delete [] buffer; // clean up
//		   return -1;
		}

		n_v6_interfaces = v6info->iAddressCount;

		for (int i = 0; i < n_v6_interfaces; ++i) {
			SOCKET_ADDRESS & addr = addrs[i];
			if (addr.iSockaddrLength >= 16) {
				boost::asio::ip::address_v6::bytes_type bts;
				for (int j = 0; j < 16; ++j)
					bts[j] = ((sockaddr_in6*)addr.lpSockaddr)->sin6_addr.u.Byte[j];
				boost::asio::ip::address_v6 v6a(bts);
				std::cout << STRFORMAT("IPV6 = %s\n", v6a);
			}
		}


		delete [] buffer; // clean up
	}
#endif

	boost::asio::io_service svc;
	boost::asio::ip::udp::socket usock1(svc);
	boost::asio::ip::udp::socket usock2(svc);

	boost::asio::ip::udp::endpoint bcep(boost::asio::ip::address::from_string("FF02::01"), 12345);
	boost::asio::ip::udp::endpoint lcep(boost::asio::ip::address_v6::loopback(), 0);
	boost::asio::ip::udp::endpoint anep(boost::asio::ip::address_v6::any(), 12345);
	boost::asio::ip::udp::endpoint ncep(boost::asio::ip::address::from_string("fe80::250:56ff:fec0:8"), 12345);

	try {
		usock1.open(boost::asio::ip::udp::v6());
		usock1.bind(anep);
		usock1.set_option(boost::asio::ip::multicast::join_group(bcep.address()));


		start_receive(usock1);
	} catch(...) {};
	std::cout << "sending\n";
	usock2.open(boost::asio::ip::udp::v6());
	//usock2.bind(ncep);

	//usock2.set_option(boost::asio::ip::udp::socket::broadcast(true));

	boost::asio::ip::udp::endpoint raddr;

	char buf[3];
	while (1) {
	usock2.send_to(
		boost::asio::buffer(buf),
		bcep,
		0);
/*		usock1.receive_from(
			boost::asio::buffer(buf),
			raddr,
			0);
		std::cout << "source: " << raddr << '\n';
*/
		svc.run();
	}

	std::string p;
	std::cin >> p;

}
