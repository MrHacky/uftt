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
#ifdef __APPLE__
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

using namespace std;

#define MAXBUFSIZE (10*1024)

namespace {

typedef std::pair<boost::asio::ip::address, boost::asio::ip::address> addrwbcst;

#ifdef WIN32
	set<addrwbcst> get_ipv4_list()
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

	set<boost::asio::ip::address> get_ipv6_list()
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

	set<boost::asio::ip::address> get_ipv6_list()
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
#endif

#if defined(__linux__) || defined(__APPLE__)
	set<addrwbcst> get_ipv4_list()
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

#ifndef __APPLE__
		ifr = ifc.ifc_req;
		for (int i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
#else
		for (int n = 0; n < ifc.ifc_len; ) {
			ifr = (struct ifreq*)(((char*)ifc.ifc_req) + n);
			n += sizeof(ifr->ifr_name);
			n += (ifr->ifr_addr.sa_len > sizeof(struct sockaddr)
			    ? ifr->ifr_addr.sa_len : sizeof(struct sockaddr));
#endif

			if (ifr->ifr_addr.sa_family != AF_INET) continue;

			sockaddr_in * pAddress;

			if (ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				printf("SIOCGIFFLAGS:Failed\n");
				continue;
			}
			short flags = ifr->ifr_flags;

			boost::asio::ip::address_v4::bytes_type ifaddr;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

			if (ioctl(skfd, SIOCGIFNETMASK, ifr) != 0) {
				printf("SIOCGIFNETMASK:Failed \n");
				continue;
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
#ifdef __APPLE__
	set<boost::asio::ip::address> get_ipv6_list()
	{
		return set<boost::asio::ip::address>();
	}
#endif

	template <class C, typename T>
	class ip_watcher_common {
		private:
			C* This() {
				return (C*)this;
			}

			bool closed;
		protected:
			boost::thread thread;
			boost::asio::io_service& service;
			set<T> addrlist;
#ifdef WIN32
			SOCKET fd;
#endif
#ifdef __linux__
			int fd;
#endif

		public:
			ip_watcher_common(boost::asio::io_service& service_)
			: service(service_)
			, closed(true)
			{
			}

			~ip_watcher_common()
			{
				close();
			}

			void close()
			{
				if (closed) return;
				closed = true;
				This()->watcher = NULL;
#ifdef WIN32
				::closesocket(fd);
#endif
#ifdef __linux__
				::close(fd);
#endif
			}

			static void checked_invoke_add_addr(boost::shared_ptr<ip_watcher_common<C, T> > this_, T& arg)
			{
				if (this_->This()->watcher)
					this_->This()->watcher->add_addr(arg);
			}

			static void checked_invoke_del_addr(boost::shared_ptr<ip_watcher_common<C, T> > this_, T& arg)
			{
				if (this_->This()->watcher)
					this_->This()->watcher->del_addr(arg);
			}

			static void checked_invoke_new_list(boost::shared_ptr<ip_watcher_common<C, T> > this_, std::set<T>& arg)
			{
				if (this_->This()->watcher)
					this_->This()->watcher->new_list(arg);
			}

			static void newlist(boost::shared_ptr<ip_watcher_common<C, T> > this_)
			{
				set<T> naddrlist = this_->This()->getlist();

				BOOST_FOREACH(const T& naddr, naddrlist)
					if (!this_->addrlist.count(naddr))
						this_->service.post(boost::bind(&checked_invoke_add_addr, this_, naddr));
				BOOST_FOREACH(const T& oaddr, this_->addrlist)
					if (!naddrlist.count(oaddr))
						this_->service.post(boost::bind(&checked_invoke_del_addr, this_, oaddr));
				this_->addrlist = naddrlist;
				this_->service.post(boost::bind(&checked_invoke_new_list, this_, this_->addrlist));
			}

			void async_wait(boost::shared_ptr<ip_watcher_common<C, T> > sp)
			{
				if (!closed) return;
				boost::weak_ptr<ip_watcher_common<C, T> > wp(sp);

				closed = false;
				thread = boost::thread(boost::bind(&C::main, wp)).move();
			}

			static void main(boost::weak_ptr<ip_watcher_common<C, T> > wp)
			{
				try {
					{
						boost::shared_ptr<ip_watcher_common<C, T> > this_ = wp.lock();
						if (this_)
							this_->This()->init();
						this_->newlist(this_);

#if defined(WIN32)
						if (this_->fd == INVALID_SOCKET) {
#elif defined(__linux__)
						if (this_->fd == -1) {
#else
						if (true) {
#endif
							std::cout << "error: ipv?_watcher: no valid socket\n";
							return;
						}
					}
					int sleep = 0;
					while(1) {
						boost::shared_ptr<ip_watcher_common<C, T> > this_ = wp.lock();
						if (!this_)
							return;
						if (this_->closed)
							return;
						this_->newlist(this_);
#ifdef WIN32
						int inBuffer = 0;
						int outBuffer = 0;
						DWORD outSize = 0;
						int err = WSAIoctl(this_->fd, SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
						if (err < 0) {
							cout << "error: ipv?_watcher: " << err << '\n';
							if (sleep < 60000) sleep += 250;
							boost::this_thread::sleep(boost::posix_time::milliseconds(sleep));
						}
#endif
#ifdef __linux__
						char buffer[1024];
						int r = recv(this_->fd, buffer, sizeof(buffer), 0);
						if (r <= 0) {
							cout << "error: ipv?_watcher: " << errno << '\n';
							if (sleep < 60000) sleep += 250;
							boost::this_thread::sleep(boost::posix_time::milliseconds(sleep));
						}
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
			fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

			fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
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
	impl->close();
}

void ipv4_watcher::async_wait()
{
	impl->async_wait(impl);
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
			fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#endif
#ifdef __linux__
			struct sockaddr_nl sa;

			memset (&sa, 0, sizeof(sa));
			sa.nl_family = AF_NETLINK;
			sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;

			fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
			bind(fd, (struct sockaddr*)&sa, sizeof(sa));
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
	impl->close();
}

void ipv6_watcher::async_wait()
{
	impl->async_wait(impl);
}
