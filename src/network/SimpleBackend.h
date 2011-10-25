#ifndef SIMPLE_BACKEND_H
#define SIMPLE_BACKEND_H

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/system/error_code.hpp>

#include "../net-asio/asio_file_stream.h"
#include "../net-asio/asio_http_request.h"
#include "../net-asio/asio_dgram_conn.h"
#include "../net-asio/ipaddr_watcher.h"

#include "INetModule.h"
#include "Misc.h"

#include "../Types.h"
#include "../UFTTSettings.h"
#include "../UFTTCore.h"

#include "../util/asio_timer_oneshot.h"

typedef dgram::conn_service<boost::asio::ip::udp> UDPConnService;
typedef dgram::conn<boost::asio::ip::udp> UDPConn;

struct UDPSockInfo {
	boost::asio::ip::udp::socket& sock;
	boost::asio::ip::udp::endpoint bcst_ep;

	bool is_v4;
	bool is_main;

	UDPSockInfo(boost::asio::ip::udp::socket& nsock, bool is_v4_)
	: sock(nsock), is_v4(is_v4_), is_main(false)
	{};
};
typedef boost::shared_ptr<UDPSockInfo> UDPSockInfoRef;

class ConnectionBase;
typedef boost::shared_ptr<ConnectionBase> ConnectionBaseRef;

//class SimpleTCPConnection;
typedef boost::shared_ptr<class SimpleTCPConnection> SimpleTCPConnectionRef;
typedef boost::shared_ptr<class UDPSemiConnection> UDPSemiConnectionRef;

const boost::asio::ip::udp::endpoint uftt_bcst_ep;
const UDPSockInfoRef uftt_bcst_if;

class SimpleBackend: public INetModule {
	private:
		enum {
			UDPPACK_QUERY_SHARE = 1,
			UDPPACK_REPLY_SHARE,
			UDPPACK_REPLY_UPDATE,
			UDPPACK_QUERY_PEER,
			UDPPACK_REPLY_PEER,
			UDPPACK_REPLY_UPDATE_NEW,
		};

		UFTTCore* core;

		boost::asio::io_service& service;
		std::map<boost::asio::ip::address, UDPSockInfoRef> udpsocklist;
		boost::asio::ip::tcp::acceptor tcplistener_v4;
		boost::asio::ip::tcp::acceptor tcplistener_v6;
		std::vector<ConnectionBaseRef> conlist;
		int udpfails;
		uint32 mid;

		ipv4_watcher watcher_v4;
		ipv6_watcher watcher_v6;

		boost::scoped_ptr<UDPConnService> udp_conn_service;

		#define UDP_BUF_SIZE (2048)
		uint8 recv_buf_v4[UDP_BUF_SIZE];
		uint8 recv_buf_v6[UDP_BUF_SIZE];

		boost::asio::ip::udp::endpoint recv_peer_v4;
		boost::asio::ip::udp::endpoint recv_peer_v6;

		boost::asio::ip::udp::socket udp_sock_v4;
		boost::asio::ip::udp::socket udp_sock_v6;

		UDPSockInfoRef udp_info_v4;
		UDPSockInfoRef udp_info_v6;

		std::set<boost::asio::ip::udp::endpoint> foundpeers;
		std::set<boost::asio::ip::udp::endpoint> trypeers;
		bool clearpeers;

		boost::asio::deadline_timer peerfindertimer;
		boost::posix_time::ptime lastpeerquery;
		boost::posix_time::ptime prevpeerquery;
		boost::asio::deadline_timer stun_timer;
		boost::asio::deadline_timer stun_timer2;
		boost::asio::ip::udp::endpoint stun_server;
		boost::asio::ip::udp::endpoint stun_endpoint;
		bool stun_server_found;
		int stun_retries;

		void start_peerfinder();
		void handle_peerfinder_query(const boost::system::error_code& e, boost::shared_ptr<boost::asio::http_request> request, int prog);
		void handle_peerfinder_timer(const boost::system::error_code& e);

		void start_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer);

		std::set<uint32> parseVersions(uint8* base, uint start, uint len);
		std::set<uint32> parseVersions(uint8* base, uint start, uint len, uint* end);

		void handle_discovery_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len, uint32 version);
		void handle_stun_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len);
		void handle_rudp_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len);
		void handle_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, const boost::system::error_code& e, std::size_t len);

		void send_udp_packet_to(boost::asio::ip::udp::socket& sock, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags = 0);
		void send_udp_packet_to(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags = 0);
		void send_udp_packet(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags = 0);

		void send_query(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep);
		void send_publish(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, const std::string& name, int sharever, bool isbuild = false);
		void send_publishes(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, int sharever, bool sendbuilds);

		void add_local_share(std::string name);

		void stun_new_addr();
		void stun_do_check(const boost::system::error_code& e, int retry = 0);
		void stun_send_bind(const boost::system::error_code& e);

		void new_iface(UDPSockInfoRef si);

		void add_ip_addr_ipv4(std::pair<boost::asio::ip::address, boost::asio::ip::address> addrwbcst);
		void add_ip_addr_ipv6(boost::asio::ip::address addr);

		void del_ip_addr(boost::asio::ip::address addr);

		boost::signals::connection attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb);

		boost::signal<void(const ShareInfo&)> sig_new_share;
		boost::signal<void(const TaskInfo&)> sig_new_task;

		UFTTSettingsRef settings;
	public:

		void print_addr(std::string prefix, boost::asio::ip::address a)
		{
			std::cout << prefix << a << '\n';
		}

		SimpleBackend(UFTTCore* core_);

		void download_share(const ShareID& sid, const ext::filesystem::path& dlpath);
		void dl_connect_handle(const boost::system::error_code& e, ConnectionBaseRef conn, std::string name, ext::filesystem::path dlpath, uint32 version);
		void start_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener);
		void handle_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener, SimpleTCPConnectionRef newconn, const boost::system::error_code& e);

		void start_udp_accept(UDPConnService* cservice);
		void handle_udp_accept(UDPConnService* cservice, UDPSemiConnectionRef newconn, const boost::system::error_code& e);

	public:
		// Implementation of IBackend interface
		virtual SignalConnection connectSigAddShare(const boost::function<void(const ShareInfo&)>&);
		virtual SignalConnection connectSigDelShare(const boost::function<void(const ShareID&)>&);
		virtual SignalConnection connectSigNewTask(const boost::function<void(const TaskInfo&)>&);
		virtual SignalConnection connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&);

		virtual void doRefreshShares();
		virtual void startDownload(const ShareID& sid, const ext::filesystem::path& path);

		virtual void doManualPublish(const std::string& host);
		virtual void doManualQuery(const std::string& host);

		virtual void notifyAddLocalShare(const LocalShareID& sid);
		virtual void notifyDelLocalShare(const LocalShareID& sid);

		virtual void setModuleID(uint32 mid);
};

#endif//SIMPLE_BACKEND_H
