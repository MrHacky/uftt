#include "SimpleBackend.h"

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(SimpleBackend);

#include <set>
//#include "net-asio/asio_ipx.h"

using namespace std;

void SimpleBackend::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	sig_new_share.connect(cb);
}

void SimpleBackend::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	// TODO: implement this
}

void SimpleBackend::connectSigNewTask(const boost::function<void(const TaskInfo&)>& cb)
{
	sig_new_task.connect(cb);
}

void SimpleBackend::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	service.post(boost::bind(&SimpleBackend::attach_progress_handler, this, tid, cb));
}

void SimpleBackend::doRefreshShares()
{
	service.post(boost::bind(&SimpleBackend::send_query, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT)));
}

void SimpleBackend::startDownload(const ShareID& sid, const boost::filesystem::path& path)
{
	service.post(boost::bind(&SimpleBackend::download_share, this, sid, path));
}

void SimpleBackend::download_share(const ShareID& sid, const boost::filesystem::path& dlpath)
{
	TaskInfo ret;
	std::string shareurl = sid.sid;

	ret.shareid = ret.shareinfo.id = sid;
	ret.id.mid = mid;
	ret.id.cid = conlist.size();

	size_t colonpos = shareurl.find_first_of(":");
	std::string proto = shareurl.substr(0, colonpos);
	uint32 version = atoi(proto.substr(6).c_str());
	shareurl.erase(0, proto.size()+3);
	size_t slashpos = shareurl.find_first_of("\\/");
	std::string host = shareurl.substr(0, slashpos);
	std::string share = shareurl.substr(slashpos+1);
	SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, this));
	//newconn->sig_progress.connect(handler);
	conlist.push_back(newconn);

	newconn->shareid = sid;

	boost::asio::ip::tcp::endpoint ep(my_addr_from_string(host), UFTT_PORT);
	newconn->socket.open(ep.protocol());
	std::cout << "Connecting...\n";
	newconn->socket.async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath, version));

	ret.shareinfo.name = share;
	ret.shareinfo.host = host;
	ret.shareinfo.proto = proto;
	ret.isupload = false;
	sig_new_task(ret);
}

void SimpleBackend::doManualPublish(const std::string& host)
{
	service.post(boost::bind(&SimpleBackend::send_publishes, this,
		boost::asio::ip::udp::endpoint(my_addr_from_string(host), UFTT_PORT)
	, true, true));
}

void SimpleBackend::doManualQuery(const std::string& host)
{
	service.post(boost::bind(&SimpleBackend::send_query, this,
		boost::asio::ip::udp::endpoint(my_addr_from_string(host), UFTT_PORT)
	));
}

void SimpleBackend::checkForWebUpdates()
{
	service.post(boost::bind(&SimpleBackend::check_for_web_updates, this));
}

void SimpleBackend::doSetPeerfinderEnabled(bool enabled)
{
	service.post(boost::bind(&SimpleBackend::start_peerfinder, this, enabled));
}

void SimpleBackend::setModuleID(uint32 mid)
{
	this->mid = mid;
}

void SimpleBackend::notifyAddLocalShare(const LocalShareID& sid)
{
	service.post(boost::bind(&SimpleBackend::add_local_share, this, sid.sid));
}

void SimpleBackend::notifyDelLocalShare(const LocalShareID& sid)
{
	// TODO: implement this
}

// TODO: deprecate following crap
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

	// linux interface up/down + ipv4adrr add/delete event socket
	#if 0
	{
		struct sockaddr_nl sa;

		memset (&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

		fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
		bind(fd, (struct sockaddr*)&sa, sizeof(sa));
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

	result.insert(foundpeers.begin(), foundpeers.end());

	return rtype(result.begin(), result.end());
}
