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

	__u32 ilen = in->nlmsg_len;
	if (ilen < NLMSG_LENGTH(sizeof(*ifi)))
		return unspec;
	ilen -= NLMSG_LENGTH(sizeof(*ifi));

	__u32 alen = an->nlmsg_len;
	if (alen < NLMSG_LENGTH(sizeof(*ifa)))
		return unspec;
	alen -= NLMSG_LENGTH(sizeof(*ifa));

	/* NOTE: ifi_index and ifa_index should have the same type (int), but for
	 * some reason they are not... So instead of a normal (in)equality comparison
	 * we do a bit-wise compare.
	 */
	if (ifi->ifi_index ^ ifa->ifa_index)
		return unspec;

	if (ifi->ifi_family != AF_INET6 || ifa->ifa_family != AF_INET6)
		return unspec;

	rtattr* tbi[IFLA_MAX+1];
	memset(tbi, 0, sizeof(tbi));
	parse_rtattr(tbi, IFLA_MAX, IFLA_RTA(ifi), ilen);

	if (tbi[IFLA_IFNAME] == NULL)
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
struct ipv6_osimpl: public unix_osimpl_base {
	void init()
	{
		unix_osimpl_base::init(RTMGRP_IPV6_IFADDR);
	}

	std::set<boost::asio::ip::address> getlist()
	{
		std::set<boost::asio::ip::address> result;

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

					/* NOTE: ifi_index and ifa_index should have the same type (int), but for
					 * some reason they are not... So instead of a normal (in)equality comparison
					 * we do a bit-wise compare.
					 */
					if (ifi->ifi_index ^ ifa->ifa_index)
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
};
