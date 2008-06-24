#include "SimpleBackend.h"

#include <set>

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
// NOTE: do these methods also work for ipx?
#if WIN32
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

			sockaddr_in *pAddress;
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
			//cout << " " << inet_ntoa(pAddress->sin_addr);
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

			pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
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

	// network probe for linux (untested):
	#if 0

	#include<sys/types.h>
	#include<sys/socket.h>
	#include<sys/ioctl.h>
	#include <errno.h>
	#include <net/if.h>
	{
		struct ifconf ifc;
		char buff[1024];
		struct ifreq *ifr;
		int i,skfd;
		ifc.ifc_len = sizeof(buff);
		ifc.ifc_buf = buff;
		if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
			printf("new socket failed\n");
			exit(1);
		}
		if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			printf("SIOCGIFCONF:Failed \n");
			exit(1);
		}
		ifr = ifc.ifc_req;
		for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
			printf("found device %s,\n", ifr->ifr_name,);
			}
		return 0;
	}
	#endif

	return rtype(result.begin(), result.end());
}
