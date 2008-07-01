#include "SimpleBackend.h"

#include <set>
//#include "net-asio/asio_ipx.h"

using namespace std;

void SimpleTCPConnection::getsharepath(std::string sharename)
{
	backend->getsharepath(this, sharename);
}

void SimpleTCPConnection::sig_download_ready(std::string url)
{
	backend->sig_download_ready(url);
}

std::vector<boost::asio::ip::address> SimpleBackend::get_broadcast_adresses()
{
	typedef std::vector<boost::asio::ip::address> rtype;

	// use set to avoid duplicates
	std::set<boost::asio::ip::address> result;

	// for backup...
	result.insert(boost::asio::ip::address_v4::broadcast());

// TODO: test linux implementation below
// TODO: unify win+linux implementations
// TODO: make function out of ipx method below, and figure out where to put that
// TODO: make SIO_ADDRESS_LIST_CHANGE windows IOCP async_ function
#if WIN32
	// udp interface probe for windows
	{
		SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
		if (sd == SOCKET_ERROR) {
			cerr << "Failed to get a socket. Error " << WSAGetLastError() <<
				endl;
			return rtype(result.begin(), result.end());
		}

		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
			cerr << "Failed calling WSAIoctl: error " << WSAGetLastError() <<
					endl;
			return rtype(result.begin(), result.end());
		}

		int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
		//cout << "There are " << nNumInterfaces << " interfaces:" << endl;
		for (int i = 0; i < nNumInterfaces; ++i) {
			boost::asio::ip::address_v4::bytes_type ifaddr;
			boost::asio::ip::address_v4::bytes_type nmaddr;
			boost::asio::ip::address_v4::bytes_type bcaddr;

			sockaddr_in *pAddress;
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
			//cout << " " << inet_ntoa(pAddress->sin_addr);
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

			pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
			memcpy(&bcaddr[0], &pAddress->sin_addr, 4);
			//cout << " has bcast " << inet_ntoa(pAddress->sin_addr);

			pAddress = (sockaddr_in *) & (InterfaceList[i].iiNetmask);
			//cout << " and netmask " << inet_ntoa(pAddress->sin_addr) << endl;
			memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			//cout << " Iface is ";
			u_long nFlags = InterfaceList[i].iiFlags;
			//if (nFlags & IFF_UP) cout << "up";
			//else                 cout << "down";
			//if (nFlags & IFF_POINTTOPOINT) cout << ", is point-to-point";
			//if (nFlags & IFF_LOOPBACK)     cout << ", is a loopback iface";
			//cout << ", and can do: ";
			//if (nFlags & IFF_BROADCAST) cout << "bcast ";
			//if (nFlags & IFF_MULTICAST) cout << "multicast ";
			//cout << endl;
			if (!(nFlags & IFF_LOOPBACK) && (nFlags & IFF_BROADCAST)) {
				for (int i = 0; i < 4; ++i)
					ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
				cout << "broadcast: " << naddr << '\n';
			}
		}

		//return 0;
	}
#endif

	// udp interface probe for linux (untested):
#ifdef __linux__
	{
		struct ifconf ifc;
		char buff[1024];
		struct ifreq *ifr;
		int i,skfd;
		ifc.ifc_len = sizeof(buff);
		ifc.ifc_buf = buff;
		if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
			printf("new socket failed\n");
			return rtype(result.begin(), result.end());
		}
		if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			printf("SIOCGIFCONF:Failed \n");
			return rtype(result.begin(), result.end());
		}
		ifr = ifc.ifc_req;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
			boost::asio::ip::address_v4::bytes_type ifaddr;
			boost::asio::ip::address_v4::bytes_type nmaddr;

			if (ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				printf("SIOCGIFFLAGS:Failed \n");
				return rtype(result.begin(), result.end());
			}
			short flags = ifr->ifr_flags;

			sockaddr_in * pAddress;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);


			if (ioctl(skfd, SIOCGIFNETMASK, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				return rtype(result.begin(), result.end());
			}

			memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			if (ioctl(skfd, SIOCGIFBRDADDR, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				return rtype(result.begin(), result.end());
			}
			//cout << " has bcast " << inet_ntoa(pAddress->sin_addr);

			// if true....
			if (flags&IFF_UP && flags&IFF_BROADCAST && !(flags&IFF_LOOPBACK))
			{
				for (int i = 0; i < 4; ++i)
					ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
				cout << "broadcast: " << naddr << '\n';
			}
		}
		//return 0;
	}
#endif

	// ipx interface probe for windows
	#if 0
	{
		SOCKET sd = WSASocket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX, 0, 0, 0);
		if (sd == SOCKET_ERROR) {
			cerr << "Failed to get a socket. Error " << WSAGetLastError() <<
				endl;
			return rtype(result.begin(), result.end());
		}

		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		struct _SOCKET_ADDRESS_LIST {
	INT iAddressCount;
	SOCKET_ADDRESS Address[20];
} SOCKET_ADDRESS_LIST, FAR * LPSOCKET_ADDRESS_LIST;



		if (WSAIoctl(sd, SIO_ADDRESS_LIST_QUERY, 0, 0, &SOCKET_ADDRESS_LIST,
				sizeof(SOCKET_ADDRESS_LIST), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
			cout << "Failed calling WSAIoctl: error " << WSAGetLastError() <<
					endl;
			return rtype(result.begin(), result.end());
		}

		int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
		//cout << "There are " << nNumInterfaces << " interfaces:" << endl;
		for (int i = 0; i < SOCKET_ADDRESS_LIST.iAddressCount; ++i) {
			boost::asio::ipx::endpoint nep;
			if (nep.capacity() <= SOCKET_ADDRESS_LIST.Address[i].iSockaddrLength)
				memcpy(nep.data(), &SOCKET_ADDRESS_LIST.Address[i], SOCKET_ADDRESS_LIST.Address[i].iSockaddrLength);
			boost::asio::ipx::address naddr = nep.getAddress();
			//cout << naddr.network;
			cout << '-';
			//cout << naddr.node;
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
	}
	#endif

	return rtype(result.begin(), result.end());
}
