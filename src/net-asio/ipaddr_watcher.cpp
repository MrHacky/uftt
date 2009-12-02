#include "ipaddr_watcher.h"

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <set>
#include <iostream>

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
#  include "../linux/libnetlink.h"
}
#  include <net/if.h>
#  include <net/if_arp.h>
#endif

using namespace std;

#define MAXBUFSIZE (10*1024)

namespace {

typedef std::pair<boost::asio::ip::address, boost::asio::ip::address> addrwbcst;

#ifdef WIN32
	set<addrwbcst> win32_get_ipv4_list()
	{
		set<addrwbcst> result;
		SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == SOCKET_ERROR) {
			cout << "win32_get_ipv4_list: Failed to get a socket. Error " << WSAGetLastError() << endl;
			return result;
		}

		DWORD bytes = MAXBUFSIZE;
		std::vector<char> buffer(bytes);
		INTERFACE_INFO* InterfaceList = (INTERFACE_INFO*)&buffer[0];

		int status = WSAIoctl(sock,
			SIO_GET_INTERFACE_LIST,
			0, 0,
			InterfaceList,
			bytes, &bytes,
			0, 0);

		closesocket(sock);
		if (status == SOCKET_ERROR) {
			cout << "win32_get_ipv6_list: WSAIoctl(2) failed: " << WSAGetLastError() << '\n';
			return result;
		}

		int nNumInterfaces = bytes / sizeof(INTERFACE_INFO);
		for (int i = 0; i < nNumInterfaces; ++i) {
			u_long nFlags = InterfaceList[i].iiFlags;
			if ((nFlags & IFF_UP) && !(nFlags & IFF_LOOPBACK)) {
				sockaddr_in *pAddress;

				boost::asio::ip::address_v4::bytes_type ifaddr;
				pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
				memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

				boost::asio::ip::address_v4::bytes_type nmaddr;
				pAddress = (sockaddr_in *) & (InterfaceList[i].iiNetmask);
				memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

				boost::asio::ip::address_v4::bytes_type bcaddr;
				for (int i = 0; i < 4; ++i)
					bcaddr[i] = ifaddr[i] | ~nmaddr[i];

				boost::asio::ip::address_v4 naddr(ifaddr);
				boost::asio::ip::address_v4 baddr(bcaddr);
				result.insert(addrwbcst(naddr, baddr));
			}
		}

		return result;
	}

	set<boost::asio::ip::address> win32_get_ipv6_list()
	{
		set<boost::asio::ip::address> result;
#if defined(SIO_ADDRESS_LIST_QUERY)
		// Get an DGRAM socket to test with.
		SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET) {
			cout << "win32_get_ipv6_list: failed to get socket: " << WSAGetLastError() << '\n';
			return result;
		}

		DWORD bytes = MAXBUFSIZE;
		std::vector<char> buffer(bytes);

		LPSOCKET_ADDRESS_LIST v6info = (LPSOCKET_ADDRESS_LIST)&buffer[0];
		SOCKET_ADDRESS* addrs = (SOCKET_ADDRESS*)&v6info->Address;

		int status = WSAIoctl(sock,
			SIO_ADDRESS_LIST_QUERY,
			0, 0,
			v6info,
			bytes, &bytes,
			0, 0);

		closesocket(sock);
		if (status == SOCKET_ERROR) {
			cout << "win32_get_ipv6_list: WSAIoctl(2) failed: " << WSAGetLastError() << '\n';
			return result;
		}

		int n_v6_interfaces = v6info->iAddressCount;

		for (int i = 0; i < n_v6_interfaces; ++i) {
			SOCKET_ADDRESS& addr = addrs[i];
			if (addr.iSockaddrLength >= 16) {
				boost::asio::ip::address_v6::bytes_type bts;
				for (int j = 0; j < 16; ++j)
					bts[j] = ((sockaddr_in6*)addr.lpSockaddr)->sin6_addr.u.Byte[j];
				int scopeid = 0;
				if (addr.iSockaddrLength >= 28)
					scopeid =((sockaddr_in6*)addr.lpSockaddr)->sin6_scope_id;
				boost::asio::ip::address_v6 v6a(bts, scopeid);
				if (!v6a.is_loopback())
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

	boost::asio::ip::address_v6 get_ipv6_address(const nlmsghdr *in, const nlmsghdr *an)
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

		struct nlmsg_list *linfo = NULL;
		struct nlmsg_list *ainfo = NULL;
		struct rtnl_handle rth;

		if (rtnl_open(&rth, 0) < 0) {
			std::cout << "rtnl_open failed\n";
			return result;
		}

		if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETLINK) < 0) {
			std::cout << "cannot send dump request\n";
			rtnl_close(&rth);
			return result;
		}

		if (rtnl_dump_filter(&rth, &store_nlmsg, &linfo, NULL, NULL) < 0) {
			std::cout << "dump terminated\n";
			rtnl_close(&rth);
			return result;
		}

		if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR) < 0) {
			std::cout << "cannot send dump request\n";
			rtnl_close(&rth);
			return result;
		}

		if (rtnl_dump_filter(&rth, &store_nlmsg, &ainfo, NULL, NULL) < 0) {
			std::cout << "dump terminated\n";
			rtnl_close(&rth);
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
				boost::asio::ip::address_v6 addr = get_ipv6_address(&l->h, &a->h);
				if (!addr.is_unspecified() && !addr.is_loopback())
					result.insert(addr);
			}
		}

		rtnl_close(&rth);

		for (nlmsg_list* l=linfo; l; ) {
			nlmsg_list* o = l;
			l = l->next;
			free(o);
		}

		for (nlmsg_list* a=ainfo; a; ) {
			nlmsg_list* o = a;
			a = a->next;
			free(o);
		}

		return result;
	}

	set<addrwbcst> linux_get_ipv4_list()
	{
		set<addrwbcst> result;

		struct ifconf ifc;
		vector<char> buffer(MAXBUFSIZE);
		struct ifreq *ifr;
		int skfd;
		ifc.ifc_len = buffer.size();
		ifc.ifc_buf = &buffer[0];
		if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
			cout << "new socket failed\n";
			return result;
		}
		if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			cout << "SIOCGIFCONF:Failed \n";
			close(skfd);
			return result;
		}
		ifr = ifc.ifc_req;
		for (int i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
			sockaddr_in * pAddress;

			if (ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				printf("SIOCGIFFLAGS:Failed \n");
				close(skfd);
				return result;
			}
			short flags = ifr->ifr_flags;

			boost::asio::ip::address_v4::bytes_type ifaddr;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

			if (ioctl(skfd, SIOCGIFNETMASK, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				return result;
			}

			boost::asio::ip::address_v4::bytes_type nmaddr;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			if (flags&IFF_UP && !(flags&IFF_LOOPBACK))
			{
				boost::asio::ip::address_v4::bytes_type bcaddr;
				for (int i = 0; i < 4; ++i)
					bcaddr[i] = ifaddr[i] | ~nmaddr[i];

				boost::asio::ip::address_v4 naddr(ifaddr);
				boost::asio::ip::address_v4 baddr(bcaddr);
				result.insert(addrwbcst(naddr, baddr));
			}
		}
		close(skfd);
		return result;
	}
#endif

	set<addrwbcst> get_ipv4_list()
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

	template <class C, typename T>
	class ip_watcher_common {
		private:
			C* This() {
				return (C*)this;
			}
		public:
			boost::thread thread;
			boost::asio::io_service& service;
			set<T> addrlist;
			boost::asio::ip::udp::socket sock; // win32
			int fd; // linux

			ip_watcher_common(boost::asio::io_service& service_)
			: service(service_), sock(service_)
			{
			}

			void newlist()
			{
				set<T> naddrlist = This()->getlist();

				BOOST_FOREACH(const T& naddr, naddrlist)
					if (!addrlist.count(naddr))
						service.post(boost::bind(boost::ref(This()->watcher->add_addr), naddr));
				BOOST_FOREACH(const T& oaddr, addrlist)
					if (!naddrlist.count(oaddr))
						service.post(boost::bind(boost::ref(This()->watcher->del_addr), oaddr));
				addrlist = naddrlist;
				service.post(boost::bind(boost::ref(This()->watcher->new_list), addrlist));
			}

			void async_wait()
			{
				thread = boost::thread(boost::bind(&C::main, This())).move();
			}

			void main()
			{
				try {
					newlist();
					This()->init();
					while(1) {
#ifdef WIN32
						int inBuffer = 0;
						int outBuffer = 0;
						DWORD outSize = 0;
						int err = WSAIoctl( sock.native(), SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
						if (!(err < 0))
							newlist();
						else
							cout << "error: ipv?_watcher: " << err << '\n';
#endif
#ifdef __linux__
						char buffer[1024];
						size_t r = recv(fd, buffer, sizeof(buffer), 0);
						if (r > 0)
							newlist();
						else
							cout << "error: ipv?_watcher: " << errno << '\n';
#endif
					}
				} catch (std::exception& e) {
					cout << "fatal: ipv?_watcher: " << e.what() << '\n';
				}
			}

	};

} // anon namespace

class ipv4_watcher::implementation : public ip_watcher_common<ipv4_watcher::implementation, addrwbcst> {
	public:
		ipv4_watcher* watcher;

		implementation(boost::asio::io_service& service_)
		: ip_watcher_common<ipv4_watcher::implementation, addrwbcst>(service_), watcher(NULL)
		{
		}

		void init() {
#ifdef WIN32
			sock.open(boost::asio::ip::udp::v4());
#endif
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

			fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
			char buffer[1024];
#endif
		}

		set<addrwbcst> getlist()
		{
			return get_ipv4_list();
		}
};

ipv4_watcher::ipv4_watcher(boost::asio::io_service& service)
: impl(new implementation(service))
{
	impl->watcher = const_cast<ipv4_watcher*>(this);
}

ipv4_watcher::~ipv4_watcher()
{
}

void ipv4_watcher::async_wait()
{
	impl->async_wait();
}


class ipv6_watcher::implementation : public ip_watcher_common<ipv6_watcher::implementation, boost::asio::ip::address> {
	public:
		ipv6_watcher* watcher;

		implementation(boost::asio::io_service& service_)
		: ip_watcher_common<ipv6_watcher::implementation, boost::asio::ip::address>(service_), watcher(NULL)
		{
		}

		void init() {
#ifdef WIN32
			sock.open(boost::asio::ip::udp::v6());
#endif
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;

			fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
			char buffer[1024];
#endif
		}

		set<boost::asio::ip::address> getlist()
		{
			return get_ipv6_list();
		}
};

ipv6_watcher::ipv6_watcher(boost::asio::io_service& service)
: impl(new implementation(service))
{
	impl->watcher = const_cast<ipv6_watcher*>(this);
}

ipv6_watcher::~ipv6_watcher()
{
}

void ipv6_watcher::async_wait()
{
	impl->async_wait();
}
