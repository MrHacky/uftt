#ifndef SIMPLE_BACKEND_H
#define SIMPLE_BACKEND_H

#include "../Types.h"

#define UFTT_PORT (47189)

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string.hpp>

#include "../net-asio/asio_file_stream.h"
#include "../net-asio/asio_http_request.h"
#include "../net-asio/ipaddr_watcher.h"

#include "../UFTTSettings.h"

#include "../AutoUpdate.h"

#include "../util/StrFormat.h"
#include "../util/asio_timer_oneshot.h"

#include "../UFTTCore.h"
#include "INetModule.h"

#include "../Globals.h"

#include "Misc.h"

struct UDPSockInfo {
	boost::asio::ip::udp::socket& sock;
	boost::asio::ip::udp::endpoint bcst_ep;

	bool is_v4;

	UDPSockInfo(boost::asio::ip::udp::socket& nsock, bool is_v4_)
	: sock(nsock), is_v4(is_v4_)
	{};
};
typedef boost::shared_ptr<UDPSockInfo> UDPSockInfoRef;

class SimpleTCPConnection;
typedef boost::shared_ptr<SimpleTCPConnection> SimpleTCPConnectionRef;

const boost::asio::ip::udp::endpoint uftt_bcst_ep;
const UDPSockInfoRef uftt_bcst_if;

class SimpleBackend: public INetModule {
	private:
		UFTTCore* core;

		boost::asio::io_service& service;
		services::diskio_service& diskio;
		std::map<boost::asio::ip::address, UDPSockInfoRef> udpsocklist;
		boost::asio::ip::tcp::acceptor tcplistener_v4;
		boost::asio::ip::tcp::acceptor tcplistener_v6;
		std::vector<SimpleTCPConnectionRef> conlist;
		int udpretries;
		uint32 mid;

		ipv4_watcher watcher_v4;
		ipv6_watcher watcher_v6;

		#define UDP_BUF_SIZE (1024)
		uint8 recv_buf_v4[UDP_BUF_SIZE];
		uint8 recv_buf_v6[UDP_BUF_SIZE];

		boost::asio::ip::udp::endpoint recv_peer_v4;
		boost::asio::ip::udp::endpoint recv_peer_v6;

		boost::asio::ip::udp::socket udp_sock_v4;
		boost::asio::ip::udp::socket udp_sock_v6;

		UDPSockInfoRef udp_info_v4;
		UDPSockInfoRef udp_info_v6;

		std::set<boost::asio::ip::udp::endpoint> foundpeers;
		boost::asio::deadline_timer peerfindertimer;

		void handle_peerfinder_query(const boost::system::error_code& e, boost::shared_ptr<boost::asio::http_request> request)
		{
			if (e) {
				std::cout << "Failed to get simple global peer list: " << e.message() << '\n';
			} else {
				const std::vector<uint8>& page = request->getContent();

				uint8 content_start[] = "*S*T*A*R*T*\r";
				uint8 content_end[] = "*S*T*O*P*\r";

				typedef std::vector<uint8>::const_iterator vpos;

				vpos spos = std::search(page.begin(), page.end(), content_start   , content_start    + sizeof(content_start   ) - 1);
				vpos epos = std::search(page.begin(), page.end(), content_end     , content_end      + sizeof(content_end     ) - 1);

				if (spos == page.end() || epos == page.end()) {
					std::cout << "Error parsing global peer list.";
					start_peerfinder();
					return;
				}

				spos += sizeof(content_start) - 1;

				std::string content(spos, epos);

				std::vector<std::string> lines;
				boost::split(lines, content, boost::is_any_of("\r\n"));

				std::set<boost::asio::ip::address> addrs;

				settings.foundpeers.clear();
				BOOST_FOREACH(const std::string& line, lines) {
					std::vector<std::string> cols;
					boost::split(cols, line, boost::is_any_of("\t"));
					if (cols.size() >= 2) {
						settings.foundpeers.insert(cols[0] + "\t" + cols[1]);
					}
				}
			}

			start_peerfinder();
		}

		void handle_peerfinder_timer(const boost::system::error_code& e)
		{
			if (e) {
				std::cout << "handle_peerfinder_timer: " << e.message() << '\n';
			} else {
				if (settings.enablepeerfinder) {
					settings.prevpeerquery = settings.lastpeerquery;
					settings.lastpeerquery = boost::posix_time::second_clock::universal_time();

					std::string url;
					//url = "http://hackykid.heliohost.org/site/bootstrap.php";
					url = "http://hackykid.awardspace.com/site/bootstrap.php";
					url += "?reg=1&type=simple&class=1wdvhi09ehvnazmq23jd";

					boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, url));
					request->setHandler(boost::bind(&SimpleBackend::handle_peerfinder_query, this, boost::asio::placeholders::error, request));
				}
			}
		}

		void start_peerfinder(bool enabled)
		{
			if (settings.enablepeerfinder != enabled) {
				settings.enablepeerfinder = enabled;
				start_peerfinder();
			}
		}

		void start_peerfinder()
		{
			if (!settings.enablepeerfinder) return;

			boost::posix_time::ptime dl;
			if (settings.lastpeerquery - settings.prevpeerquery > boost::posix_time::minutes(55))
				dl = settings.lastpeerquery + boost::posix_time::seconds(20);
			else
				dl = settings.lastpeerquery + boost::posix_time::minutes(50);

			peerfindertimer.expires_at(dl);
			peerfindertimer.async_wait(boost::bind(&SimpleBackend::handle_peerfinder_timer, this, _1));

			std::cout << "Peer check at: " << my_datetime_to_string(dl) << '\n';

			uint8 udp_send_buf[5];
			udp_send_buf[0] = (1 >>  0) & 0xff;
			udp_send_buf[1] = (1 >>  8) & 0xff;
			udp_send_buf[2] = (1 >> 16) & 0xff;
			udp_send_buf[3] = (1 >> 24) & 0xff;
			udp_send_buf[4] = 4;

			boost::system::error_code err;
			BOOST_FOREACH(const std::string peer, settings.foundpeers) {
				try {
					boost::asio::ip::udp::endpoint ep;
					size_t colonpos = peer.find_first_of("\t");
					if (colonpos == std::string::npos) {
						ep.address(my_addr_from_string(peer));
						ep.port(UFTT_PORT);
					} else {
						ep.address(my_addr_from_string(peer.substr(0, colonpos)));
						ep.port(atoi(peer.substr(colonpos+1).c_str()));
					}
					send_udp_packet(uftt_bcst_if, ep, boost::asio::buffer(udp_send_buf), err);
				} catch (...) {
				}
			}
		}

		void start_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer)
		{
			si->sock.async_receive_from(boost::asio::buffer(recv_buf, UDP_BUF_SIZE), *recv_peer,
				boost::bind(&SimpleBackend::handle_udp_receive, this, si, recv_buf, recv_peer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
			);
		}

		std::set<uint32> parseVersions(uint8* base, uint start, uint len)
		{
			uint end;
			return parseVersions(base, start, len, &end);
		}

		std::set<uint32> parseVersions(uint8* base, uint start, uint len, uint* end)
		{
			std::set<uint32> versions;
			uint32 i = start;
			if (i < len) {
				uint max = base[i++];
				for (uint num = 0; num < max && i+8 <= len; ++num, i += 8) {
					uint32 lver = (base[i+0] <<  0) | (base[i+1] <<  8) | (base[i+2] << 16) | (base[i+3] << 24);
					uint32 hver = (base[i+4] <<  0) | (base[i+5] <<  8) | (base[i+6] << 16) | (base[i+7] << 24);
					if (lver <= hver && (hver-lver) < 100)
						for (uint32 j = lver; j <= hver; ++j)
							versions.insert(j);
				}
			}
			if (versions.empty()) versions.insert(1);
			*end = i-start;
			return versions;
		}

		void handle_discovery_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len, uint32 version)
		{
			std::cout << "got packet type " << (int)recv_buf[4] << " from " << *recv_peer << "\n";
			switch (recv_buf[4]) {
				case 1: if (version == 1) { // type = broadcast;
					uint32 vstart = 5;
					if (len > 5)
						vstart = 6 + recv_buf[5];

					std::set<uint32> versions = parseVersions(recv_buf, vstart, len);

					       if (versions.count(5)) {
						send_publishes(si, *recv_peer, 3, true);
					} else if (versions.count(4)) {
						send_publishes(si, *recv_peer, 2, true);
					} else if (versions.count(3)) {
						send_publishes(si, *recv_peer, 0, true);
					} else if (versions.count(2)) {
						send_publishes(si, *recv_peer, 1, true);
					} else if (versions.count(1)) {
						send_publishes(si, *recv_peer, 1, len >= 6 && recv_buf[5] > 0 && len-6 >= recv_buf[5]);
					}
				}; break;
				case 2: { // type = reply;
					if (len >= 6) {
						uint32 slen = recv_buf[5];
						if (len >= slen+6) {
							std::set<uint32> versions;
							uint vlen = 0;
							if (version == 1)
								versions = parseVersions(recv_buf, slen+6, len, &vlen);
							versions.insert(version);
							uint32 sver = 0;
							if (versions.count(2) || versions.count(3)) // Protocol 2 and 3 send shares (sver) in the same way
								sver = 2;
							else if (versions.count(1))
								sver = 1;

							if (sver > 0) {
								std::string sname((char*)recv_buf+6, (char*)recv_buf+6+slen);
								std::string surl = STRFORMAT("uftt-v%d://%s/%s", sver, recv_peer->address().to_string(), sname);
								ShareInfo sinfo;
								sinfo.name = sname;
								sinfo.proto = STRFORMAT("uftt-v%d", sver);
								sinfo.host = recv_peer->address().to_string();
								sinfo.id.sid = surl;
								sinfo.id.mid = mid;
								if(versions.count(3)) { // Version 3 added support for usernames (nicknames)
									int nickname_length = recv_buf[slen + 6 + vlen];
									std::string nickname((char*)recv_buf + 6 + slen + 1 + vlen, (char*)recv_buf + 6 + slen + 1 + vlen + nickname_length);
									sinfo.user = nickname;
								}
								sig_new_share(sinfo);
							}
						}
					}
				}; break;
				case 3: { // type = autoupdate share
					if (len >= 6 && (version == 1)) {
						uint32 slen = recv_buf[5];
						if (len >= slen+6) {
							std::string sname((char*)recv_buf+6, (char*)recv_buf+6+slen);
							std::string surl = STRFORMAT("uftt-v%d://%s/%s", 1, recv_peer->address().to_string(), sname);;
							ShareInfo sinfo;
							sinfo.name = sname;
							sinfo.proto = STRFORMAT("uftt-v%d", 1);
							sinfo.host = recv_peer->address().to_string();
							sinfo.id.sid = surl;
							sinfo.id.mid = mid;
							sinfo.isupdate = true;
							sig_new_share(sinfo);
						}
					}
				}; break;
				case 4: { // peerfinder query
					boost::system::error_code err;
					uint8 udp_send_buf[5];
					udp_send_buf[0] = (1 >>  0) & 0xff;
					udp_send_buf[1] = (1 >>  8) & 0xff;
					udp_send_buf[2] = (1 >> 16) & 0xff;
					udp_send_buf[3] = (1 >> 24) & 0xff;
					udp_send_buf[4] = 5;
					send_udp_packet(si, *recv_peer, boost::asio::buffer(udp_send_buf), err);
				};// intentional fallthrough break;
				case 5: { // peerfinder reply
					foundpeers.insert(*recv_peer);
					settings.foundpeers.insert(STRFORMAT("%s\t%d", recv_peer->address(), recv_peer->port()));
					send_query(si, *recv_peer);
				}; break;
			}
		}

		void handle_stun_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len)
		{
			uint16 type = (recv_buf[0] << 8) | (recv_buf[1] << 0);
			uint16 slen = (recv_buf[2] << 8) | (recv_buf[3] << 0);
			std::cout << "Got STUN packet\n";
		}

		void handle_rudp_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len)
		{
		}

		void handle_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, const boost::system::error_code& e, std::size_t len) {
			if (!e) {
				udpretries = 10;
				if (len >= 4) {
					uint32 rpver = (recv_buf[0] <<  0) | (recv_buf[1] <<  8) | (recv_buf[2] << 16) | (recv_buf[3] << 24);
					if ((rpver&0xfffffffc) == 0) {
						handle_discovery_packet(si, recv_buf, recv_peer, len, rpver);
					} else if (len >= 20 && (rpver&0x030000c0) == 0) {
						handle_stun_packet(si, recv_buf, recv_peer, len);
					} else if ((rpver&0xff000000) == 0x01000000) {
						rpver = (recv_buf[0] <<  0) | (recv_buf[1] <<  8);
						handle_discovery_packet(si, recv_buf, recv_peer, len, rpver);
					} else if ((rpver&0xff000000) == 0x02000000) {
						handle_rudp_packet(si, recv_buf, recv_peer, len);
					}
				}
			} else {
				if (udpretries == 10) {
					// ignore first message
				} else
					std::cout << "udp receive failed: " << e.message() << '\n';
				--udpretries;
			}

			if (!e || udpretries > 0)
				start_udp_receive(si, recv_buf, recv_peer);
			else
				std::cout << "retry limit reached, giving up on receiving udp packets\n";
		}

		void send_query(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep) {
			uint8 udp_send_buf[1024];
			udp_send_buf[ 0] = 0x01;
			udp_send_buf[ 1] = 0x00;
			udp_send_buf[ 2] = 0x00;
			udp_send_buf[ 3] = 0x00;
			udp_send_buf[ 4] = 0x01;
			udp_send_buf[ 5] = 0x01;
			udp_send_buf[ 6] = 0x00;
			udp_send_buf[ 7] = 0x00;

			std::set<uint32> qversions;
			qversions.insert(1);
			qversions.insert(2);
			qversions.insert(4);
			qversions.insert(5);

			int plen = 8;
			BOOST_FOREACH(uint32 ver, qversions) {
				udp_send_buf[plen+0] = udp_send_buf[plen+4] = (ver >>  0) & 0xff;
				udp_send_buf[plen+1] = udp_send_buf[plen+5] = (ver >>  8) & 0xff;
				udp_send_buf[plen+2] = udp_send_buf[plen+6] = (ver >> 16) & 0xff;
				udp_send_buf[plen+3] = udp_send_buf[plen+7] = (ver >> 24) & 0xff;
				++udp_send_buf[7];
				plen += 8;
			}

			boost::system::error_code err;
			send_udp_packet(si, ep, boost::asio::buffer(udp_send_buf, plen), err);
			if (err)
				std::cout << "qeury to '" << ep << "' failed: " << err.message() << '\n';
		}

		/**
		 * send_publish announces our list of shares to the network.
		 * Note, there is a hack implemented here to also support older versions of the protocol.
		 * The hack is that we reuse the version number 1. This is because in version 1 uftt did
		 * not include backward/forward version info. So we send the packet as version 1 and add
		 * the extend info at the end. Old version will ignore the extra data while new versions
		 * will parse this and everybody was happy :-)
		 **/
		void send_publish(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, const std::string& name, int sharever, bool isbuild = false)
		{
			uint8 udp_send_buf[1024];
			udp_send_buf[0] = (sharever >>  0) & 0xff;
			udp_send_buf[1] = (sharever >>  8) & 0xff;
			udp_send_buf[2] = (sharever >> 16) & 0xff;
			udp_send_buf[3] = (sharever >> 24) & 0xff;
			udp_send_buf[4] = isbuild ? 3 : 2;
			udp_send_buf[5] = name.size();
			memcpy(&udp_send_buf[6], name.data(), name.size());

			uint32 plen = name.size()+6;
			//advertise which versions of the share we support
			if (!isbuild && sharever == 1 && ep == uftt_bcst_ep) {
				std::set<uint32> sversions;
				sversions.insert(1);
				sversions.insert(2);
				sversions.insert(3);

				udp_send_buf[plen++] = sversions.size();

				BOOST_FOREACH(uint32 ver, sversions) {
					udp_send_buf[plen+0] = udp_send_buf[plen+4] = (ver >>  0) & 0xff;
					udp_send_buf[plen+1] = udp_send_buf[plen+5] = (ver >>  8) & 0xff;
					udp_send_buf[plen+2] = udp_send_buf[plen+6] = (ver >> 16) & 0xff;
					udp_send_buf[plen+3] = udp_send_buf[plen+7] = (ver >> 24) & 0xff;
					//++udp_send_buf[7];
					plen += 8;
				}
			}

			if(sharever == 1 || sharever >= 3) { // Version 3 added support for usernames (nicknames)
				#undef min
				int nickname_length = std::min((size_t)255, settings.nickname.size());
				udp_send_buf[plen++] = nickname_length;
				memcpy(&udp_send_buf[plen], settings.nickname.data(), nickname_length);
				plen += nickname_length;
			}

			boost::system::error_code err;
			send_udp_packet(si, ep, boost::asio::buffer(udp_send_buf, plen), err);
			if (err)
				std::cout << "publish of '" << name << "' failed: " << err.message() << '\n';
		}

		void send_publishes(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, int sharever, bool sendbuilds) {
			if (sharever > 0) {
				std::vector<std::string> local_shares = core->getLocalShares();
				BOOST_FOREACH(const std::string& item, local_shares)
					if (item.size() < 0xff)
						send_publish(si, ep, item, sharever);
			}
			if (sendbuilds)
				BOOST_FOREACH(const std::string& name, updateProvider.getAvailableBuilds())
					send_publish(si, ep, name, 1, true);
		}

		void add_local_share(std::string name)
		{
			send_publish(uftt_bcst_if, uftt_bcst_ep, name, 1);
		}

		template<typename BUF>
		void send_udp_packet_to(boost::asio::ip::udp::socket& sock, const boost::asio::ip::udp::endpoint& ep, BUF buf, boost::system::error_code& err, int flags = 0)
		{
			++udpretries;
			sock.send_to(
				buf,
				ep,
				flags,
				err);
		}

		template<typename BUF>
		void send_udp_packet_to(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, BUF buf, boost::system::error_code& err, int flags = 0)
		{
			std::cout << "Send udp packet to " << ep << "(" << si->is_v4 << ")\n";
			if (si->is_v4 == ep.address().is_v4())
				send_udp_packet_to(si->sock, ep, buf, err, flags);
			else
				std::cout << "Skipped\n";
		}

		template<typename BUF>
		void send_udp_packet(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, BUF buf, boost::system::error_code& err, int flags = 0)
		{
			if (!si) { // == uftt_bcst_if
				typedef std::pair<boost::asio::ip::address, UDPSockInfoRef> sipair;
				BOOST_FOREACH(sipair sip, udpsocklist) if (sip.second) {
					send_udp_packet(sip.second, ep, buf, err, flags);
					if (err)
						std::cout << "send to (" << ep << ") failed: " << err.message() << '\n';
				}
			} else {
				if (ep == uftt_bcst_ep)
					send_udp_packet_to(si, si->bcst_ep, buf, err, flags);
				else
					send_udp_packet_to(si, ep, buf, err, flags);
			}
		}

		UFTTSettings& getSettings() {
			return settings;
		}

		void new_iface(UDPSockInfoRef si)
		{
			send_publishes(si, uftt_bcst_ep, 1, true);
			send_query    (si, uftt_bcst_ep);

			asio_timer_oneshot(service, (int)1000, boost::bind(&SimpleBackend::send_publishes, this, si, uftt_bcst_ep, 1, true));
			asio_timer_oneshot(service, (int)1000, boost::bind(&SimpleBackend::send_query    , this, si, uftt_bcst_ep));
		}

		void add_ip_addr_ipv4(std::pair<boost::asio::ip::address, boost::asio::ip::address> addrwbcst) {
			boost::asio::ip::address addr = addrwbcst.first;
			boost::asio::ip::address bcaddr = addrwbcst.second;
			if (!udpsocklist[addr]) {
				UDPSockInfoRef newsock(new UDPSockInfo(udp_info_v4->sock, true));
				newsock->bcst_ep = boost::asio::ip::udp::endpoint(bcaddr, UFTT_PORT);
				udpsocklist[addr] = newsock;
				new_iface(newsock);
			} else
				std::cout << "Duplicate added adress: " << addr << '\n';
		}

		void add_ip_addr_ipv6(boost::asio::ip::address addr) {
			if (!udpsocklist[addr]) {
				try {
					UDPSockInfoRef newsock(new UDPSockInfo(udp_info_v6->sock, false));
					boost::asio::ip::address_v6 bcaddr = my_addr_from_string("ff02::1").to_v6();
					bcaddr.scope_id(addr.to_v6().scope_id());
					newsock->sock.set_option(boost::asio::ip::multicast::join_group(bcaddr, bcaddr.scope_id()));
					newsock->bcst_ep = boost::asio::ip::udp::endpoint(bcaddr, UFTT_PORT);

					udpsocklist[addr] = newsock;

					new_iface(newsock);
				} catch (std::exception& e) {
					std::cout << "Failed to initialise IPv6 address (" << addr << ") : " << e.what() << '\n';
				}
			} else
				std::cout << "Duplicate added adress: " << addr << '\n';
		}

		void del_ip_addr(boost::asio::ip::address addr) {
			udpsocklist.erase(addr);
		}
	public:
		UFTTSettings& settings; // TODO: remove need for this hack!

		void print_addr(std::string prefix, boost::asio::ip::address a)
		{
			std::cout << prefix << a << '\n';
		}

		SimpleBackend(UFTTCore* core_)
			: service(core_->get_io_service())
			, diskio(core_->get_disk_service())
			, tcplistener_v4(core_->get_io_service())
			, tcplistener_v6(core_->get_io_service())
			, settings(core_->getSettingsRef())
			, peerfindertimer(core_->get_io_service())
			, udpretries(10)
			, watcher_v4(core_->get_io_service())
			, watcher_v6(core_->get_io_service())
			, udp_sock_v4(core_->get_io_service())
			, udp_sock_v6(core_->get_io_service())
			, core(core_)
		{
			udp_info_v4 = UDPSockInfoRef(new UDPSockInfo(udp_sock_v4, true));
			udp_info_v6 = UDPSockInfoRef(new UDPSockInfo(udp_sock_v6, false));

			try {
				udp_sock_v6.open(boost::asio::ip::udp::v6());
				udp_sock_v6.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v6::any(), UFTT_PORT));
				udp_sock_v6.set_option(boost::asio::ip::udp::socket::broadcast(true));
				udp_sock_v6.set_option(boost::asio::ip::multicast::enable_loopback(true));

				start_udp_receive(udp_info_v6, recv_buf_v6, &recv_peer_v6);

			} catch (std::exception& e) {
				std::cout << "Failed to initialise IPv6 UDP scoket: " << e.what() << "\n";
			}

			try {
				udp_sock_v4.open(boost::asio::ip::udp::v4());
				udp_sock_v4.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), UFTT_PORT));
				udp_sock_v4.set_option(boost::asio::ip::udp::socket::broadcast(true));
				udp_sock_v4.set_option(boost::asio::ip::multicast::enable_loopback(true));

				start_udp_receive(udp_info_v4, recv_buf_v4, &recv_peer_v4);
			} catch (std::exception& e) {
				std::cout << "Failed to initialise IPv4 UDP socket: " << e.what() << "\n";
#ifdef __linux__
				if (udp_sock_v6.is_open()) {
					std::cout << "Re-using IPv6 UDP socket\n";
					udp_info_v4 = UDPSockInfoRef(new UDPSockInfo(udp_sock_v6, true));
				}
#endif
			}

			watcher_v4.add_addr.connect(boost::bind(&SimpleBackend::print_addr, this, "[IPV4] + ", boost::bind(&get_first, _1)));
			watcher_v4.del_addr.connect(boost::bind(&SimpleBackend::print_addr, this, "[IPV4] - ", boost::bind(&get_first, _1)));
			watcher_v6.add_addr.connect(boost::bind(&SimpleBackend::print_addr, this, "[IPV6] + ", _1));
			watcher_v6.del_addr.connect(boost::bind(&SimpleBackend::print_addr, this, "[IPV6] - ", _1));

			watcher_v4.add_addr.connect(boost::bind(&SimpleBackend::add_ip_addr_ipv4, this, _1));
			watcher_v4.del_addr.connect(boost::bind(&SimpleBackend::del_ip_addr, this, boost::bind(&get_first, _1)));
			watcher_v6.add_addr.connect(boost::bind(&SimpleBackend::add_ip_addr_ipv6, this, _1));
			watcher_v6.del_addr.connect(boost::bind(&SimpleBackend::del_ip_addr, this, _1));

			watcher_v4.async_wait();
			watcher_v6.async_wait();

			try {
				tcplistener_v6.open(boost::asio::ip::tcp::v6());
				tcplistener_v6.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v6::any(), UFTT_PORT));
				tcplistener_v6.listen(16);
				start_tcp_accept(&tcplistener_v6);
			} catch (std::exception& e) {
				std::cout << "Failed to initialize IPv6 TCP listener: " << e.what() << '\n';
			}

			try {
				tcplistener_v4.open(boost::asio::ip::tcp::v4());
				tcplistener_v4.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), UFTT_PORT));
				tcplistener_v4.listen(16);
				start_tcp_accept(&tcplistener_v4);
			} catch (std::exception& e) {
				std::cout << "Failed to initialize IPv4 TCP listener: " << e.what() << '\n';
			}

			// bind autoupdater
			updateProvider.newbuild.connect(
				boost::bind(&SimpleBackend::send_publish, this, uftt_bcst_if, uftt_bcst_ep, _1, 1, true)
			);

			start_peerfinder();
		}

		boost::filesystem::path getsharepath(std::string name) {
			return core->getLocalSharePath(name); // TODO: remove need for this hack!
		}

		void download_share(const ShareID& sid, const boost::filesystem::path& dlpath);
		void dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath, uint32 version);
		void start_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener);
		void handle_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener, SimpleTCPConnectionRef newconn, const boost::system::error_code& e);
		void attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb);

	private:
		// old interface now..
		boost::signal<void(const ShareInfo&)> sig_new_share;
		boost::signal<void(const TaskInfo&)> sig_new_task;

	public:
		// new interface starts here!

		virtual void connectSigAddShare(const boost::function<void(const ShareInfo&)>&);
		virtual void connectSigDelShare(const boost::function<void(const ShareID&)>&);
		virtual void connectSigNewTask(const boost::function<void(const TaskInfo&)>&);
		virtual void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&);

		virtual void doRefreshShares();
		virtual void startDownload(const ShareID& sid, const boost::filesystem::path& path);

		virtual void doManualPublish(const std::string& host);
		virtual void doManualQuery(const std::string& host);

		virtual void notifyAddLocalShare(const LocalShareID& sid);
		virtual void notifyDelLocalShare(const LocalShareID& sid);

		virtual void doSetPeerfinderEnabled(bool enabled);

		virtual void setModuleID(uint32 mid);
};

#endif//SIMPLE_BACKEND_H
