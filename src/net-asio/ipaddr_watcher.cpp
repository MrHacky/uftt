#include "ipaddr_watcher.h"

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <set>

#ifdef WIN32
#endif
#ifdef __linux__
#  include <asm/types.h>
#  include <linux/netlink.h>
#  include <linux/rtnetlink.h>
#  include <sys/socket.h>
#  include <linux/if_addr.h>
#  include <linux/if_link.h>
extern "C" {
#  include "../bench/libnetlink.h"
}
#  include <net/if.h>
#  include <net/if_arp.h>
#endif

using namespace std;

namespace {

#ifdef WIN32
	set<boost::asio::ip::address> win32_get_ipv4_list()
	{
		set<boost::asio::ip::address> result;
		SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
		if (sd == SOCKET_ERROR) {
			cerr << "Failed to get a socket. Error " << WSAGetLastError() <<
				endl;
			return result;
		}

		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
			cerr << "Failed calling WSAIoctl: error " << WSAGetLastError() <<
					endl;
			return result;
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
				//for (int i = 0; i < 4; ++i)
				//	ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
				//cout << "broadcast: " << naddr << '\n';
			}
		}

		return result;
	}

	set<boost::asio::ip::address> win32_get_ipv6_list()
	{
		set<boost::asio::ip::address> result;
#if defined(SIO_ADDRESS_LIST_QUERY)
		int n_v6_interfaces = 0;

		LPSOCKET_ADDRESS_LIST v6info;
		SOCKET_ADDRESS* addrs;
		DWORD buflen = sizeof (SOCKET_ADDRESS_LIST) + (63 * sizeof (SOCKET_ADDRESS ));
		std::vector<char> buffer(buflen);

		v6info = reinterpret_cast<LPSOCKET_ADDRESS_LIST> (&buffer[0]);
		addrs = reinterpret_cast<SOCKET_ADDRESS *> (&buffer[0] + sizeof (SOCKET_ADDRESS_LIST));

		// Get an (overlapped) DGRAM socket to test with.
		// If it fails only return IPv4 interfaces.
		SOCKET sock = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET)
		 {
		   return result;
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
			return result;
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
				result.insert(v6a);
			}
		}
#endif
		return result;
	}

#endif

#ifdef __linux__
	struct nlmsg_list {
		struct nlmsg_list *next;
		struct nlmsghdr h;
	};

	const char *rt_addr_n2a(int af, int  len,
			void *addr, char *buf, int buflen)
	{
		switch (af) {
		case AF_INET:
		case AF_INET6:
			return inet_ntop(af, addr, buf, buflen);
		default:
			return "???";
		}
	}

	int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
	{
		struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
		struct nlmsg_list *h;
		struct nlmsg_list **lp;

		//std::cout << "msg: " << n->nlmsg_len << std::endl;

		h = (struct nlmsg_list *)malloc(n->nlmsg_len+sizeof(void*));
		if (h == NULL)
			return -1;

		memcpy(&h->h, n, n->nlmsg_len);
		h->next = NULL;

		for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
		*lp = h;

		//ll_remember_index(who, n, NULL);
		return 0;
	}

	boost::asio::ip::address_v6 get_address_string(const nlmsghdr *in, const nlmsghdr *an)
	//std::string get_address_string(const nlmsghdr *in, const nlmsghdr *an)
	{
		boost::asio::ip::address_v6 unspec;
		if (in->nlmsg_type != RTM_NEWLINK)
			return unspec;
		if (an->nlmsg_type != RTM_NEWADDR)
			return unspec;

		ifinfomsg* ifi = (ifinfomsg*)NLMSG_DATA(in);
		ifaddrmsg* ifa = (ifaddrmsg*)NLMSG_DATA(an);

		int ilen = in->nlmsg_len;
		ilen -= NLMSG_LENGTH(sizeof(*ifi));
		if (ilen < 0)
			return unspec;

		int alen = an->nlmsg_len;
		if (alen < NLMSG_LENGTH(sizeof(ifa)))
			 return unspec;

		if (ifi->ifi_index != ifa->ifa_index)
			return unspec;

		if (ifi->ifi_family != AF_INET6 || ifa->ifa_family != AF_INET6)
			return unspec;

		rtattr* tbi[IFLA_MAX+1];
		memset(tbi, 0, sizeof(tbi));
		parse_rtattr(tbi, IFLA_MAX, IFLA_RTA(ifi), ilen);

		if (tbi[IFLA_IFNAME] == NULL)
			return unspec;

		alen -= NLMSG_LENGTH(sizeof(*ifa));
		if (alen < 0)
			return unspec;

		rtattr* tba[IFA_MAX+1];
		memset(tba, 0, sizeof(tba));
		parse_rtattr(tba, IFA_MAX, IFA_RTA(ifa), alen);

		char abuf[256];
		rt_addr_n2a(ifa->ifa_family,
				RTA_PAYLOAD(tba[IFA_ADDRESS]),
				RTA_DATA(tba[IFA_ADDRESS]),
				abuf, sizeof(abuf));
		std::string ipaddr = abuf;

		try {
			boost::asio::ip::address_v6 addr(boost::asio::ip::address_v6::from_string(ipaddr));
			addr.scope_id(ifi->ifi_index);
			return addr;
		} catch(...) {
			return unspec;
		}
	}

	set<boost::asio::ip::address> linux_get_ipv6_list()
	{
		set<boost::asio::ip::address> result;

		static const char *const option[] = { "to", "scope", "up", "label", "dev", 0 };

		struct nlmsg_list *linfo = NULL;
		struct nlmsg_list *ainfo = NULL;
		struct rtnl_handle rth;
		char *filter_dev = NULL;
		int no_link = 0;

		if (rtnl_open(&rth, 0) < 0) {
			std::cout << "rtnl_open failed\n";
			return result;
		}

		if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETLINK) < 0) {
			std::cout << "cannot send dump request\n";
			return result;
		}

		if (rtnl_dump_filter(&rth, &store_nlmsg, &linfo, NULL, NULL) < 0) {
			std::cout << "dump terminated\n";
			return result;
		}

		if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR) < 0) {
			std::cout << "cannot send dump request\n";
			return result;
		}

		if (rtnl_dump_filter(&rth, &store_nlmsg, &ainfo, NULL, NULL) < 0) {
			std::cout << "dump terminated\n";
			return result;
		}

		{
			struct nlmsg_list **lp;
			lp=&linfo;
			nlmsg_list* l;
			while ((l=*lp)!=NULL) {
				int ok = 0;
				struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(&l->h);
				struct nlmsg_list *a;

				for (a=ainfo; a; a=a->next) {
					struct nlmsghdr *n = &a->h;
					struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(n);

					if (ifa->ifa_index != ifi->ifi_index)
						continue;
					if (ifa->ifa_family != AF_INET6)
						continue;

					ok = 1;
					break;
				}
				if (!ok)
					*lp = l->next;
				else
					lp = &l->next;
			}
		}

		for (nlmsg_list* l=linfo; l; l = l->next) {
			for (nlmsg_list* a=ainfo; a; a = a->next) {
				boost::asio::ip::address_v6 addr = get_address_string(&l->h, &a->h);
				if (!addr.is_unspecified() && !addr.is_loopback())
					result.insert(addr);
			}
		}

		return result;
	}

	set<boost::asio::ip::address> linux_get_ipv4_list()
	{
		set<boost::asio::ip::address> result;

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
			//boost::asio::ip::address_v4::bytes_type nmaddr;

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

			//memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			// if true....
			if (flags&IFF_UP && flags&IFF_BROADCAST && !(flags&IFF_LOOPBACK))
			{
				//for (int i = 0; i < 4; ++i)
				//	ifaddr[i] |= ~nmaddr[i];
				boost::asio::ip::address_v4 naddr(ifaddr);
				result.insert(naddr);
			}
		}
		return result;
	}
#endif

	set<boost::asio::ip::address> get_ipv4_list()
	{
#ifdef WIN32
		return win32_get_ipv4_list();
#endif
#ifdef __linux__
		return linux_get_ipv4_list();
#endif
	}

	set<boost::asio::ip::address> get_ipv6_list()
	{
#ifdef WIN32
		return win32_get_ipv6_list();
#endif
#ifdef __linux__
		return linux_get_ipv6_list();
#endif
	}

} // anon namespace

class ipv4_watcher::implementation {
	public:
		boost::asio::io_service& service;
		boost::asio::ip::udp::socket sock;
		boost::thread thread;
		ipv4_watcher* watcher;
		set<boost::asio::ip::address> addrlist;

		implementation(boost::asio::io_service& service_, ipv4_watcher* watcher_)
		: service(service_), watcher(watcher_), sock(service_)
		{
		}

		void async_wait()
		{
			thread = boost::thread(boost::bind(&ipv4_watcher::implementation::main, this)).move();
		}

		void main() {
			sock.open(boost::asio::ip::udp::v4());
			newlist();
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

			int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
			char buffer[1024];

#endif
			while(1) {
#ifdef WIN32
				int inBuffer = 0;
				int outBuffer = 0;
				DWORD outSize = 0;
				int err = WSAIoctl( sock.native(), SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
				if (!(err < 0))
					newlist();
				else
					cout << "error: ipv4_watcher: " << err << '\n';
#endif
#ifdef __linux__
				//std::cout << "recv:";
				size_t r = recv(fd, buffer, sizeof(buffer), 0);
				//std::cout << r << '\n';
				newlist();
#endif
			}
		}

		void newlist()
		{
			set<boost::asio::ip::address> naddrlist = getlist();

			BOOST_FOREACH(const boost::asio::ip::address& naddr, naddrlist)
				if (!addrlist.count(naddr))
					service.post(boost::bind(boost::ref(watcher->add_addr), naddr));
			BOOST_FOREACH(const boost::asio::ip::address& oaddr, addrlist)
				if (!naddrlist.count(oaddr))
					service.post(boost::bind(boost::ref(watcher->del_addr), oaddr));
			addrlist = naddrlist;
			service.post(boost::bind(boost::ref(watcher->new_list), addrlist));
		}

		set<boost::asio::ip::address> getlist()
		{
			return get_ipv4_list();
		}
};

ipv4_watcher::ipv4_watcher(boost::asio::io_service& service)
: impl(new implementation(service, this))
{
}

ipv4_watcher::~ipv4_watcher()
{
}

void ipv4_watcher::async_wait()
{
	impl->async_wait();
}


class ipv6_watcher::implementation {
	public:
		boost::asio::io_service& service;
		boost::asio::ip::udp::socket sock;
		boost::thread thread;
		ipv6_watcher* watcher;
		set<boost::asio::ip::address> addrlist;

		implementation(boost::asio::io_service& service_, ipv6_watcher* watcher_)
		: service(service_), watcher(watcher_), sock(service_)
		{
		}

		void async_wait()
		{
			thread = boost::thread(boost::bind(&ipv6_watcher::implementation::main, this)).move();
		}

		void main() {
			sock.open(boost::asio::ip::udp::v6());
			newlist();
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;

			int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
			char buffer[1024];

#endif
			while(1) {
#ifdef WIN32
				int inBuffer = 0;
				int outBuffer = 0;
				DWORD outSize = 0;
				int err = WSAIoctl( sock.native(), SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
				if (!(err < 0))
					newlist();
				else
					cout << "error: ipv4_watcher: " << err << '\n';
#endif
#ifdef __linux__
				//std::cout << "recv:";
				size_t r = recv(fd, buffer, sizeof(buffer), 0);
				//std::cout << r << '\n';
				newlist();
#endif

			}
		}

		void newlist()
		{
			set<boost::asio::ip::address> naddrlist = getlist();

			BOOST_FOREACH(const boost::asio::ip::address& naddr, naddrlist)
				if (!addrlist.count(naddr))
					service.post(boost::bind(boost::ref(watcher->add_addr), naddr));
			BOOST_FOREACH(const boost::asio::ip::address& oaddr, addrlist)
				if (!naddrlist.count(oaddr))
					service.post(boost::bind(boost::ref(watcher->del_addr), oaddr));
			addrlist = naddrlist;
			service.post(boost::bind(boost::ref(watcher->new_list), addrlist));
		}

		set<boost::asio::ip::address> getlist()
		{
			return get_ipv6_list();
		}
};

ipv6_watcher::ipv6_watcher(boost::asio::io_service& service)
: impl(new implementation(service, this))
{
}

ipv6_watcher::~ipv6_watcher()
{
}

void ipv6_watcher::async_wait()
{
	impl->async_wait();
}
