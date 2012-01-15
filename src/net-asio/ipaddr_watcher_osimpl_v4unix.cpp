#if defined(__linux__) && !defined(ANDROID)
#  define HAVE_LINUX_SYNCWAIT
#endif

#ifdef HAVE_LINUX_SYNCWAIT
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

// hmz, seems quite a bit too linux specific to be called 'unix'?
struct unix_osimpl_base {
	int fd;

	void init(int ifaddrflag)
	{
#ifdef HAVE_LINUX_SYNCWAIT
		struct sockaddr_nl sa;

		memset (&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_groups = RTMGRP_LINK | ifaddrflag;//(isv6 ? RTMGRP_IPV6_IFADDR : RTMGRP_IPV6_IFADDR;

		fd = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
		::bind(fd, (struct sockaddr*)&sa, sizeof(sa));
#else
		fd = -1;
#endif
	}

	bool sync_wait()
	{
#ifdef HAVE_LINUX_SYNCWAIT
		if (fd == -1) return false;
		char buffer[1024];
		int r = ::recv(fd, buffer, sizeof(buffer), 0);
		if (r <= 0) {
			std::cout << "error: ipv?_watcher: " << errno << '\n';
			return false;
		}
		return true;
#else
		return false;
#endif
	}

	void close()
	{
		::close(fd);
	}

	bool cancel_wait()
	{
		close();
		return false;
	}
};

struct ipv4_osimpl: public unix_osimpl_base {
	void init()
	{
#ifdef HAVE_LINUX_SYNCWAIT
		unix_osimpl_base::init(RTMGRP_IPV4_IFADDR);
#endif
	}

	std::set<addrwithbroadcast> getlist()
	{
		std::set<addrwithbroadcast> result;

		struct ifconf ifc;
		std::vector<char> buffer(10*1024); //MAXBUFSIZE
		struct ifreq *ifr;
		int skfd;
		ifc.ifc_len = buffer.size();
		ifc.ifc_buf = &buffer[0];
		if ((skfd = ::socket(AF_INET, SOCK_DGRAM,0)) < 0) {
			std::cout << "new socket failed\n";
			return result;
		}
		if (::ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
			std::cout << "SIOCGIFCONF:Failed\n";
			::close(skfd);
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

			if (::ioctl(skfd, SIOCGIFFLAGS, ifr) != 0) {
				std::cout << "SIOCGIFFLAGS:Failed\n";
				continue;
			}
			short flags = ifr->ifr_flags;

			boost::asio::ip::address_v4::bytes_type ifaddr;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&ifaddr[0], &pAddress->sin_addr, 4);

			if (::ioctl(skfd, SIOCGIFNETMASK, ifr) != 0) {
				std::cout << "SIOCGIFNETMASK:Failed\n";
				continue;
			}

			boost::asio::ip::address_v4::bytes_type nmaddr;
			pAddress = (sockaddr_in *) &ifr->ifr_addr;
			memcpy(&nmaddr[0], &pAddress->sin_addr, 4);

			if (flags&IFF_UP && !(flags&IFF_LOOPBACK)) {
				boost::asio::ip::address_v4::bytes_type bcaddr;
				for (int i = 0; i < 4; ++i)
					bcaddr[i] = ifaddr[i] | ~nmaddr[i];

				boost::asio::ip::address_v4 naddr(ifaddr);
				boost::asio::ip::address_v4 baddr(bcaddr);
				result.insert(addrwithbroadcast(naddr, baddr));
			}
		}
		::close(skfd);
		return result;
	}
};
