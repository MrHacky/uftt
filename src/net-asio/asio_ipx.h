#ifndef ASIO_IPX
#define ASIO_IPX

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#  pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/push_options.hpp>

#include <boost/asio/basic_datagram_socket.hpp>

// symbols needed from platform headers:
// - AF_IPX
// - NSPROTO_IPX
// - SOCKADDR_IPX
//   - .sipx_node
//   - .sipx_port
//   - .sipx_network
//   - .sipx_family

#ifdef WIN32
	#include <Wsipx.h>

	#define sipx_node sa_nodenum
	#define sipx_port sa_socket
	#define sipx_network sa_netnum
	#define sipx_family sa_family
#else
	#include <netipx/ipx.h>

	#define NSPROTO_IPX PF_IPX
	#define SOCKADDR_IPX sockaddr_ipx
#endif

namespace boost {
namespace asio {

namespace detail {
namespace ipx {
	class endpoint;
	class address;
}
}

/// Encapsulates the flags needed for IPX.
/**
 * The boost::asio::ipx::ipx class contains flags necessary for IPX sockets.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 *
 * @par Concepts:
 * Protocol
 */
class ipx
{
public:
  /// The type of a UDP endpoint.
  typedef basic_datagram_socket<ipx> socket;
  typedef detail::ipx::endpoint endpoint;
  typedef detail::ipx::address address;

  /// Obtain an identifier for the type of the protocol.
  int type() const
  {
    return SOCK_DGRAM;
  }

  /// Obtain an identifier for the protocol.
  int protocol() const
  {
    return NSPROTO_IPX;
  }

  /// Obtain an identifier for the protocol family.
  int family() const
  {
    return AF_IPX;
  }
};

namespace detail {
namespace ipx {

class address {
	public:
		boost::array<unsigned char, 4> network;
		boost::array<unsigned char, 6> node;

		bool operator<(const address&o) {
			if (network < o.network) return true;
			if (network > o.network) return false;
			return node < o.node;
		}

	//static public:
		static address broadcast_all()
		{
			address ret;
			for (int i = 0; i < 4; ++i)
				ret.network[i] = 0xff;
			for (int i = 0; i < 6; ++i)
				ret.node[i] = 0xff;
			return ret;
		}

		static address broadcast()
		{
			address ret;
			for (int i = 0; i < 4; ++i)
				ret.network[i] = 0x00;
			for (int i = 0; i < 6; ++i)
				ret.node[i] = 0xff;
			return ret;
		}

		static address local()
		{
			address ret;
			for (int i = 0; i < 4; ++i)
				ret.network[i] = 0;
			for (int i = 0; i < 6; ++i)
				ret.node[i] = 0;
			return ret;
		}
};

class endpoint {
public:
	typedef boost::asio::ipx protocol_type;

	endpoint()
	{
		memset(&saddr, 0, sizeof(saddr));
		saddr.sipx_family = AF_IPX;
	}

	endpoint(const address& addr, const unsigned short port)
	{
		memset(&saddr, 0, sizeof(saddr));
		saddr.sipx_family = AF_IPX;
		// no host<->network byte order conversion for net/node num?
		for (int i = 0; i < 4; ++i)
			((char*)&saddr.sipx_network)[i] = addr.network[i]; // TODO: verify/fix LE/BE problem
		for (int i = 0; i < 6; ++i)
			saddr.sipx_node[i] = addr.node[i];
		saddr.sipx_port = htons(port);
	}

	protocol_type::address getAddress() const
	{
		return address();
	}

	protocol_type::address address() const
	{
		protocol_type::address ret;
		// no host<->network byte order conversion for net/node num?
		for (int i = 0; i < 4; ++i)
			ret.network[i] = ((char*)&saddr.sipx_network)[i]; // TODO: verify/fix LE/BE problem
		for (int i = 0; i < 6; ++i)
			ret.node[i] = saddr.sipx_node[i];
		return ret;
	}

	unsigned short port() const
	{
		return ntohs(saddr.sipx_port);
	}

	protocol_type protocol() const
	{
		return protocol_type();
	}

	sockaddr* data()
	{
		return (sockaddr*)&saddr;
	}

	const sockaddr* data() const
	{
		return (sockaddr*)&saddr;
	}

	size_t size() const
	{
		return 2+4+6+2; // sizeof(addr); // 14?
	}

	void resize(size_t s)
	{
		if (s > capacity())
			throw std::runtime_error("invalid size");
	}

	size_t capacity() const
	{
		return sizeof(saddr); // 14?
	}

	std::ostream& operator<<(std::ostream& os)
	{
	}

	bool operator==(const endpoint& o) const {
		return (0 == memcmp(this, &o, size()));
	}

private:
	SOCKADDR_IPX saddr;

};

} // namespace ipx
} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif//ASIO_IPX
