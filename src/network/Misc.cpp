#include "Misc.h"
#ifndef WIN32 // Linux?
	#include <net/if.h>
#endif

boost::asio::ip::address my_addr_from_string(const std::string& str) {
	if (str == "255.255.255.255")
		return boost::asio::ip::address_v4::broadcast();
#ifndef WIN32 // Linux?
	else if(str.find_first_of("%") != std::string::npos) {
		std::string str_ = str.substr(0, str.find_first_of("%"));
		boost::asio::ip::address_v6 addr = boost::asio::ip::address_v6::from_string(str_);

		std::string ifname = str.substr(str.find_first_of("%")+ 1);

		//in6_addr_type* ipv6_address = static_cast<in6_addr_type*>(addr);
		//bool is_link_local = IN6_IS_ADDR_LINKLOCAL(ipv6_address);
		unsigned long scope_id = 0;
		//if (is_link_local)
			scope_id = if_nametoindex(ifname.c_str());
		if (scope_id == 0)
			scope_id = atoi(ifname.c_str());

		addr.scope_id(scope_id);

		return addr;
	}
#endif
	else
		return boost::asio::ip::address::from_string(str);
}
