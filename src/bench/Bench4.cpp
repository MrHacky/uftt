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
#include "../net-asio/ipaddr_watcher.h"

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

static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
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

static int print_linkinfo(struct sockaddr_nl  *who,
		const struct nlmsghdr *n, void  *arg)
{
	FILE *fp = (FILE*)arg;
	struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	int len = n->nlmsg_len;
	unsigned m_flag = 0;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL) {
		printf("nil ifname");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELLINK)
		printf("Deleted ");

	std::cout << "link = ";

	printf("%d: %s", ifi->ifi_index,
		tb[IFLA_IFNAME] ? (char*)RTA_DATA(tb[IFLA_IFNAME]) : "<nil>");

	std::cout << std::endl;

	return 0;
}

static int print_addrinfo(struct sockaddr_nl  *who,
		const struct nlmsghdr *n, void  *arg)
{
	//FILE *fp = (FILE*)arg;
	struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * rta_tb[IFA_MAX+1];
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;
	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0) {
		printf("wrong nlmsg len %d", len);
		return -1;
	}

	std::cout << "print_addrinfo.len = " << len << std::endl;

	memset(rta_tb, 0, sizeof(rta_tb));
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

	std::cout << "addr = ";

	printf( " peer %s/%d ",
	rt_addr_n2a(ifa->ifa_family,
		    RTA_PAYLOAD(rta_tb[IFA_ADDRESS]),
		    RTA_DATA(rta_tb[IFA_ADDRESS]),
		    abuf, sizeof(abuf)),
	ifa->ifa_prefixlen);

	std::cout << std::endl;
}

std::string get_address_string(const nlmsghdr *in, const nlmsghdr *an)
{
	if (in->nlmsg_type != RTM_NEWLINK)
		return "";
	if (an->nlmsg_type != RTM_NEWADDR)
		return "";

	ifinfomsg* ifi = (ifinfomsg*)NLMSG_DATA(in);
	ifaddrmsg* ifa = (ifaddrmsg*)NLMSG_DATA(an);

	int ilen = in->nlmsg_len;
	ilen -= NLMSG_LENGTH(sizeof(*ifi));
	if (ilen < 0)
		return "";

	int alen = an->nlmsg_len;
	if (alen < NLMSG_LENGTH(sizeof(ifa)))
		 return "";

	if (ifi->ifi_index != ifa->ifa_index)
		return "";

	if (ifi->ifi_family != AF_INET6 || ifa->ifa_family != AF_INET6)
		return "";

	rtattr* tbi[IFLA_MAX+1];
	memset(tbi, 0, sizeof(tbi));
	parse_rtattr(tbi, IFLA_MAX, IFLA_RTA(ifi), ilen);

	if (tbi[IFLA_IFNAME] == NULL)
		return "";

	std::string linkname = (char*)RTA_DATA(tbi[IFLA_IFNAME]);

	if (linkname == "lo")
		return "";

	alen -= NLMSG_LENGTH(sizeof(*ifa));
	if (alen < 0)
		return "";

	rtattr* tba[IFA_MAX+1];
	memset(tba, 0, sizeof(tba));
	parse_rtattr(tba, IFA_MAX, IFA_RTA(ifa), alen);

	char abuf[256];
	rt_addr_n2a(ifa->ifa_family,
		    RTA_PAYLOAD(tba[IFA_ADDRESS]),
		    RTA_DATA(tba[IFA_ADDRESS]),
		    abuf, sizeof(abuf));
	std::string ipaddr = abuf;

	return ipaddr + "%" + linkname;
}

static int print_selected_addrinfo(int ifindex, struct nlmsg_list *ainfo, FILE *fp)
{
	std::cout << "saddr" << std::endl;
	for ( ;ainfo ;  ainfo = ainfo->next) {
		struct nlmsghdr *n = &ainfo->h;
		struct ifaddrmsg *ifa = (struct ifaddrmsg*)NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			return -1;

		if (ifa->ifa_index != ifindex || ifa->ifa_family != AF_INET6)
			continue;

		print_addrinfo(NULL, n, fp);
	}
	return 0;
}

// initially ripped from libiproute/ipadress.c (GPL)
int ipaddr_list_or_flush()
{
	static const char *const option[] = { "to", "scope", "up", "label", "dev", 0 };

	struct nlmsg_list *linfo = NULL;
	struct nlmsg_list *ainfo = NULL;
	struct rtnl_handle rth;
	char *filter_dev = NULL;
	int no_link = 0;

	if (rtnl_open(&rth, 0) < 0)
		exit(1);

	if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETLINK) < 0) {
		printf("cannot send dump request");
	}

	if (rtnl_dump_filter(&rth, &store_nlmsg, &linfo, NULL, NULL) < 0) {
		printf("dump terminated");
	}

	if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR) < 0) {
		printf("cannot send dump request");
	}

	if (rtnl_dump_filter(&rth, &store_nlmsg, &ainfo, NULL, NULL) < 0) {
		printf("dump terminated");
	}

	if (true) {
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
			std::string addrstr = get_address_string(&l->h, &a->h);
			if (!addrstr.empty()) {
				std::cout << "addr: " << addrstr << '\n';
			}
		}
	}

	exit(0);
}

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

void print_addr(std::string prefix, boost::asio::ip::address a)
{
	std::cout << prefix << a << '\n';
}

int main( int argc, char **argv ) {
	boost::asio::io_service svc;
	boost::asio::io_service::work wrk(svc);
	ipv4_watcher w4(svc);
	ipv6_watcher w6(svc);

	w4.add_addr.connect(boost::bind(&print_addr, "[IPV4] + ", _1));
	w4.del_addr.connect(boost::bind(&print_addr, "[IPV4] - ", _1));
	w6.add_addr.connect(boost::bind(&print_addr, "[IPV6] + ", _1));
	w6.del_addr.connect(boost::bind(&print_addr, "[IPV6] - ", _1));

	w4.async_wait();
	w6.async_wait();

	while(1)
		svc.run();

#ifdef __linux__
	{
		std::set<boost::asio::ip::address> addrs;
		addrs = linux_list_ipv4_adresses();
		BOOST_FOREACH(const boost::asio::ip::address& a, addrs)
			cout << "IPV4= " << a << '\n';
		//addrs = linux_list_ipv6_adresses();
		//BOOST_FOREACH(const boost::asio::ip::address& a, addrs)
		//	cout << "IPV6= " << a << '\n';
	}
	ipaddr_list_or_flush();
#endif
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
