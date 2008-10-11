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

#include <set>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include "../util/StrFormat.h"

#ifdef __linux__
#  include <asm/types.h>
#  include <linux/netlink.h>
#  include <linux/rtnetlink.h>
#  include <sys/socket.h>
#endif

using namespace std;
#ifdef __linux__
	std::set<boost::asio::ip::address> linux_list_ipv4_adresses()
	{
		std::set<boost::asio::ip::address> result;
		struct ifconf ifc;
		char buff[1024];
		struct ifreq *ifr;
		int i,skfd;
		ifc.ifc_len = sizeof(buff);
		ifc.ifc_buf = buff;
		if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
			printf("new socket failed\n");
			return result;
		}
		if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			printf("SIOCGIFCONF:Failed \n");
			return result;
		}
		ifr = ifc.ifc_req;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
			boost::asio::ip::address_v4::bytes_type ifaddr;
			boost::asio::ip::address_v4::bytes_type nmaddr;

			if (ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				printf("SIOCGIFFLAGS:Failed \n");
				return result;
			}
			short flags = ifr->ifr_flags;

			sockaddr_in * pAddress;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);


			if (ioctl(skfd, SIOCGIFNETMASK, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				return result;
			}

			memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			if (ioctl(skfd, SIOCGIFBRDADDR, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				return result;
			}
			//cout << " has bcast " << inet_ntoa(pAddress->sin_addr);

			// if true....
			if (flags&IFF_UP && flags&IFF_BROADCAST && !(flags&IFF_LOOPBACK))
			{
				for (int i = 0; i < 4; ++i)
					ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
				//cout << "broadcast: " << naddr << '\n';
			}
		}
		//return 0;
		return result;
	}

	std::set<boost::asio::ip::address> linux_list_ipv6_adresses()
	{
		std::set<boost::asio::ip::address> result;
		struct ifconf ifc;
		char buff[1024];
		struct ifreq *ifr;
		int i,skfd;
		ifc.ifc_len = sizeof(buff);
		ifc.ifc_buf = buff;
		if ((skfd = socket(AF_INET6, SOCK_DGRAM,0)) < 0) {
			printf("new socket failed\n");
			return result;
		}
		if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			printf("SIOCGIFCONF:Failed \n");
			return result;
		}
		ifr = ifc.ifc_req;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
			boost::asio::ip::address_v6::bytes_type ifaddr;
			int scopeid = 0;

			if (ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				printf("SIOCGIFFLAGS:Failed \n");
				return result;
			}
			short flags = ifr->ifr_flags;

			sockaddr_in6 * pAddress;
			pAddress = (sockaddr_in6 *) &ifr->ifr_addr;
			for (int j = 0; j < 16; ++j)
				ifaddr[j] = pAddress->sin6_addr.s6_addr[16-j];
			//memcpy(&ifaddr[0], &pAddress->sin6_addr, 16);
			scopeid = pAddress->sin6_scope_id;

			//if (flags&IFF_UP && flags&IFF_BROADCAST && !(flags&IFF_LOOPBACK))
			//{
				boost::asio::ip::address_v6 naddr(ifaddr, scopeid);
				result.insert(naddr);
				//cout << "broadcast: " << naddr << '\n';
			//}
		}
		//return 0;
		return result;
	}
#endif

char udp_recv_buf[10];
boost::asio::ip::udp::endpoint udp_recv_addr;

void start_receive(boost::asio::ip::udp::socket& s);

bool my_is_v4_isatap(const boost::asio::ip::address_v6& addr)
{
	return STRFORMAT("%s", addr).substr(0, 11) == "fe80::5efe:";
}

void do_receive(boost::asio::ip::udp::socket& s, const boost::system::error_code& e, size_t len ) {
	std::cout << "source: " << udp_recv_addr << '\n';

	start_receive(s);
}

void start_receive(boost::asio::ip::udp::socket& s) {
	s.async_receive_from(boost::asio::buffer(udp_recv_buf), udp_recv_addr,
		boost::bind(&do_receive, boost::ref(s), _1, _2)
		);
}

template <typename Proto>
class address_watcher {
	boost::thread t;
	boost::function<void()> nf;
	typedef typename Proto::socket socktype;
	socktype& sock;

	void main() {
#ifdef WIN32
		while (1) {
			int inBuffer = 0;
			int outBuffer = 0;
			DWORD outSize = 0;
			//int err = ioctlsocket(sock.native(), SIO_ADDRESS_LIST_CHANGE, NULL);
			int err = WSAIoctl( sock.native(), SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
			if (! (err < 0))
				sock.io_service().post(nf);
		}
#endif
	}
public:
	address_watcher(socktype& sock_)
		: sock(sock_)
	{
	}

	void async_watch(const boost::function<void()>& f) {
		nf = f;
		t = boost::thread(boost::bind(&address_watcher<Proto>::main, this)).move();
	}

};

std::set<boost::asio::ip::address> oaddrsv6;
void addrlister() {
	std::set<boost::asio::ip::address> addrsv6;
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
				std::vector<unsigned char> cbuf(addr.iSockaddrLength);
				for (int j = 0; j < addr.iSockaddrLength; ++j)
					cbuf[j] = ((unsigned char*)addr.lpSockaddr)[j];
				boost::asio::ip::address_v6::bytes_type bts;
				for (int j = 0; j < 16; ++j)
					bts[j] = ((sockaddr_in6*)addr.lpSockaddr)->sin6_addr.u.Byte[j];
				int scopeid = 0;
				if (addr.iSockaddrLength >= 28)
					scopeid =((sockaddr_in6*)addr.lpSockaddr)->sin6_scope_id;
				boost::asio::ip::address_v6 v6a(bts, scopeid);
				if (v6a.is_link_local()) {
					//std::cout << STRFORMAT("IPV6 = %s\n", v6a);
					addrsv6.insert(v6a);
				}
			}
		}

		delete [] buffer; // clean up
	}

	BOOST_FOREACH(const boost::asio::ip::address& naddr, addrsv6)
		if (!oaddrsv6.count(naddr))
			std::cout << STRFORMAT("+IPV6 = %s\n", naddr);
	BOOST_FOREACH(const boost::asio::ip::address& oaddr, oaddrsv6)
		if (!addrsv6.count(oaddr))
			std::cout << STRFORMAT("-IPV6 = %s\n", oaddr);
	oaddrsv6 = addrsv6;
#endif
}

int main( int argc, char **argv ) {

	boost::asio::io_service svc;
	boost::asio::ip::udp::socket usock1(svc);
	boost::asio::ip::udp::socket usock0(svc);
	usock0.open(boost::asio::ip::udp::v6());

	address_watcher<boost::asio::ip::udp> awatch(usock0);

	addrlister();
	awatch.async_watch(boost::bind(&addrlister));

#ifdef __linux__
	{
		struct sockaddr_nl sa;

		memset (&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

		int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
		bind(fd, (struct sockaddr*)&sa, sizeof(sa));
	}
	{
		std::set<boost::asio::ip::address> addrs;
		addrs = linux_list_ipv4_adresses();
		BOOST_FOREACH(const boost::asio::ip::address& a, addrs)
			cout << "IPV4= " << a << '\n';
		addrs = linux_list_ipv6_adresses();
		BOOST_FOREACH(const boost::asio::ip::address& a, addrs)
			cout << "IPV6= " << a << '\n';
	}
#endif

	boost::asio::ip::udp::endpoint bcep(boost::asio::ip::address::from_string("FF02::01"), 12345);
	boost::asio::ip::udp::endpoint bcep2(boost::asio::ip::address::from_string("FF02::01"), 12345);
	boost::asio::ip::udp::endpoint lcep(boost::asio::ip::address_v6::loopback(), 0);
	boost::asio::ip::udp::endpoint anep(boost::asio::ip::address_v6::any(), 12345);
	//boost::asio::ip::udp::endpoint ncep(boost::asio::ip::address::from_string("fe80::21d:d9ff:fe08:42a0%19"), 0);
	//boost::asio::ip::udp::endpoint ncep(boost::asio::ip::address::from_string("fe80::21a:6bff:fecd:721c%20"), 0);
	//boost::asio::ip::udp::endpoint ncep(boost::asio::ip::address::from_string("fe80::21c:26ff:fee8:96f4%10"), 0);

	cout << "receiving\n";

	try {
		usock1.open(boost::asio::ip::udp::v6());
		usock1.bind(anep);
		//usock1.bind(boost::asio::ip::udp::endpoint(ncep.address(), 12345));
		usock1.set_option(boost::asio::ip::multicast::join_group(bcep2.address()));
		usock1.set_option(boost::asio::ip::multicast::enable_loopback(true));

		start_receive(usock1);
	} catch(...) {};
	std::cout << "sending\n";
	//usock2.open(boost::asio::ip::udp::v6());
	//usock2.bind(ncep);
	//usock2.bind(ncep);


	boost::asio::ip::udp::endpoint raddr;

	boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(svc)));

	char buf[3];
	//while(1)
	for(int i = 0; i < oaddrsv6.size(); ++i) {
		//std::cout << "Send: " << addrsv6[i];
		try {
			boost::asio::ip::udp::socket usock2(svc);
			usock2.open(boost::asio::ip::udp::v6());
			//usock2.bind(boost::asio::ip::udp::endpoint(addrsv6[i],0));
			//usock2.bind(boost::asio::ip::udp::endpoint(oaddrsv6[i],0));
			//usock2.set_option(boost::asio::ip::udp::socket::broadcast(true));
			//boost::asio::ip::multicast::outbound_interface option(boost::asio::ip::address::from_string("fe80::21d:d9ff:fe08:42a0%5"))
			//usock2.set_option(boost::asio::ip::multicast::enable_loopback(true));
			//usock2.set_option(boost::asio::ip::multicast::hops(5));
			//usock2.bind(ncep);

			usock2.send_to(
			boost::asio::buffer(buf),
			bcep,
			//boost::asio::ip::udp::endpoint(ncep.address(), 12345),
			0);
			//std::cout << " Done!\n";
		} catch (...) {
			//std::cout << " Fail!\n";
		}
/*		usock1.receive_from(
			boost::asio::buffer(buf),
			raddr,
			0);
		std::cout << "source: " << raddr << '\n';
*/
	}
	//svc.run();

	std::string p;
	std::cin >> p;

}
