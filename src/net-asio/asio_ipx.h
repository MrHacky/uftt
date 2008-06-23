#ifndef ASIO_IPX
#define ASIO_IPX

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/push_options.hpp>

#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/basic_resolver_iterator.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/detail/socket_types.hpp>

#ifdef WIN32
#include <Wsipx.h>
#endif

namespace boost {
namespace asio {

namespace detail {
namespace ipx {

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
  typedef class endpoint endpoint;
  typedef class address address;

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

  /// Compare two protocols for equality.
  friend bool operator==(const ipx& p1, const ipx& p2)
  {
    return true;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const ipx& p1, const ipx& p2)
  {
    return true;
  }
};

class address {
	public:
		boost::array<unsigned char, 4> netnum;
		boost::array<unsigned char, 6> nodenum;

	static address broadcast_all()
	{
		address ret;
		for (int i = 0; i < 4; ++i)
			ret.netnum[i] = 0xff;
		for (int i = 0; i < 6; ++i)
			ret.nodenum[i] = 0xff;
		return ret;
	}

	static address broadcast_local()
	{
		address ret;
		for (int i = 0; i < 4; ++i)
			ret.netnum[i] = 0x00;
		for (int i = 0; i < 6; ++i)
			ret.nodenum[i] = 0xff;
		return ret;
	}

	static address local()
	{
		address ret;
		for (int i = 0; i < 4; ++i)
			ret.netnum[i] = 0;
		for (int i = 0; i < 6; ++i)
			ret.nodenum[i] = 0;
		return ret;
	}
};

class endpoint {
public:
	typedef ipx protocol_type;

	endpoint()
	{
		memset(&saddr, 0, sizeof(saddr));
		saddr.sa_family = AF_IPX;
	}

	endpoint(const address& addr, const unsigned short port)
	{
		memset(&saddr, 0, sizeof(saddr));
		saddr.sa_family = AF_IPX;
		// no host<->network byte order conversion for net/node num?
		for (int i = 0; i < 4; ++i)
			saddr.sa_netnum[i] = addr.netnum[i];
		for (int i = 0; i < 6; ++i)
			saddr.sa_nodenum[i] = addr.nodenum[i];
		saddr.sa_socket = htons(port);
	}

	address address() const
	{
		protocol_type::address ret;
		// no host<->network byte order conversion for net/node num?
		for (int i = 0; i < 4; ++i)
			ret.netnum[i] = saddr.sa_netnum[i];
		for (int i = 0; i < 6; ++i)
			ret.nodenum[i] = saddr.sa_nodenum[i];
		return ret;
	}

	unsigned short port() const
	{
		return ntohs(saddr.sa_socket);
	}

	protocol_type protocol() const
	{
		return ipx();
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
			throw std::exception("invalid size");
	}

	size_t capacity() const
	{
		return sizeof(saddr); // 14?
	}

private:
	SOCKADDR_IPX saddr;

};

} // namespace ipx
} // namespace detail

using detail::ipx::ipx;

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif//ASIO_IPX