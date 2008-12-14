#include "SimpleBackendIPv6.h"

#include <boost/asio.hpp>

#include "NetModuleLinker.h"
#include "SimpleBackend.h"

struct ProtoIPv6 {
	static boost::asio::ip::udp udp() { return boost::asio::ip::udp::v6(); };
	static boost::asio::ip::tcp tcp() { return boost::asio::ip::tcp::v6(); };
	static boost::asio::ip::address addr_broadcast() { return boost::asio::ip::address_v6::from_string("ff02::1"); };
	static boost::asio::ip::address addr_any() { return boost::asio::ip::address_v6::any(); };
};

typedef SimpleBackend<ProtoIPv6> SimpleBackendIPv6;

DISABLE_NETMODULE_CLASS(SimpleBackendIPv6);
