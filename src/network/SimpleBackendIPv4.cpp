#include "SimpleBackendIPv4.h"

#include <boost/asio.hpp>

#include "NetModuleLinker.h"
#include "SimpleBackend.h"

struct ProtoIPv4 {
	static boost::asio::ip::udp udp() { return boost::asio::ip::udp::v4(); };
	static boost::asio::ip::tcp tcp() { return boost::asio::ip::tcp::v4(); };
	static boost::asio::ip::address addr_broadcast() { return boost::asio::ip::address_v4::broadcast(); };
	static boost::asio::ip::address addr_any() { return boost::asio::ip::address_v4::any(); };
};

typedef SimpleBackend<ProtoIPv4> SimpleBackendIPv4;

REGISTER_NETMODULE_CLASS(SimpleBackendIPv4);
