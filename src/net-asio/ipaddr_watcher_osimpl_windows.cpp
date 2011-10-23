// common stuff for ipv4 and ipv6
struct win32_osimpl_base {
	SOCKET fd;

	bool sync_wait()
	{
		if (fd == INVALID_SOCKET) return false;
		int inBuffer = 0;
		int outBuffer = 0;
		DWORD outSize = 0;
		int err = ::WSAIoctl(fd, SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
		if (err < 0) return false;
		return true;
	}

	void close()
	{
		::closesocket(fd);
	}

	bool cancel_wait()
	{
		close();
		return false;
	}
};

struct ipv4_osimpl: public win32_osimpl_base {
	void init()
	{
		fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	std::set<addrwithbroadcast> getlist()
	{
		std::set<addrwithbroadcast> result;
		SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET) {
			std::cout << "win32_get_ipv4_list: Failed to get a socket. Error " << WSAGetLastError() << '\n';
			return result;
		}

		DWORD bytes = 10*1024;//MAXBUFSIZE;
		std::vector<char> buffer(bytes);
		INTERFACE_INFO* InterfaceList = (INTERFACE_INFO*)&buffer[0];

		int status = ::WSAIoctl(sock,
			SIO_GET_INTERFACE_LIST,
			0, 0,
			InterfaceList,
			bytes, &bytes,
			0, 0);

		::closesocket(sock);
		if (status == SOCKET_ERROR) {
			std::cout << "win32_get_ipv6_list: WSAIoctl(2) failed: " << WSAGetLastError() << '\n';
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
				result.insert(addrwithbroadcast(naddr, baddr));
			}
		}

		return result;
	}
};

struct ipv6_osimpl: public win32_osimpl_base {
	void init()
	{
		fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	}

	std::set<boost::asio::ip::address> getlist()
	{
		std::set<boost::asio::ip::address> result;
#if defined(SIO_ADDRESS_LIST_QUERY)
		// Get an DGRAM socket to test with.
		SOCKET sock = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET) {
			std::cout << "win32_get_ipv6_list: failed to get socket: " << WSAGetLastError() << '\n';
			return result;
		}

		DWORD bytes = 10*1024;//MAXBUFSIZE;
		std::vector<char> buffer(bytes);

		LPSOCKET_ADDRESS_LIST v6info = (LPSOCKET_ADDRESS_LIST)&buffer[0];
		SOCKET_ADDRESS* addrs = (SOCKET_ADDRESS*)&v6info->Address;

		int status = ::WSAIoctl(sock,
			SIO_ADDRESS_LIST_QUERY,
			0, 0,
			v6info,
			bytes, &bytes,
			0, 0);

		closesocket(sock);
		if (status == SOCKET_ERROR) {
			std::cout << "win32_get_ipv6_list: WSAIoctl(2) failed: " << WSAGetLastError() << '\n';
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
};
