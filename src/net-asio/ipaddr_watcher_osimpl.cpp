#ifdef IPADDR_WATCHER_INCLUDE

	#include <iostream>

	#ifdef WIN32
		// provides both ipv4 and ipv6 implementation
		#include "ipaddr_watcher_osimpl_windows.cpp"
	#else
		#if defined(__linux__) || defined(__APPLE__)
			#include "ipaddr_watcher_osimpl_v4unix.cpp"
		#else
			#define IPADDRW_DUMMY_IPV4
		#endif
		#ifdef __linux__
			#include "ipaddr_watcher_osimpl_v6linux.cpp"
		#else
			#define IPADDRW_DUMMY_IPV6
		#endif
	#endif

	#ifdef IPADDRW_DUMMY_IPV4
		struct ipv4_osimpl {
			void init() {};
			void close() {};
			void sync_wait() { return false; };

			std::set<addrwithbroadcast> getlist() { return std::set<addrwithbroadcast>(); };
			bool cancel_wait() { return false; };
		};
	#endif

	#ifdef IPADDRW_DUMMY_IPV4
		struct ipv6_osimpl {
			void init() {};
			void close() {};
			void sync_wait() { return false; };

			std::set<boost::asio::ip::address> getlist() { return std::set<addrwithbroadcast>(); };
			bool cancel_wait() { return false; };
		};
	#endif

#endif
