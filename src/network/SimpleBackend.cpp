#include "SimpleBackend.h"
#include "SimpleTCPConnection.h"
#include "UDPSemiConnection.h"
#include "../Globals.h"
#include "../util/StrFormat.h"
#include <boost/algorithm/string.hpp>

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(SimpleBackend);

using namespace std;

SimpleBackend::SimpleBackend(UFTTCore* core_)
	: core(core_)
	, settings(core_->getSettingsRef())
	, service(core_->get_io_service())
	, tcplistener_v4(core_->get_io_service())
	, tcplistener_v6(core_->get_io_service())
	, udpfails(0)
	, watcher_v4(core_->get_io_service())
	, watcher_v6(core_->get_io_service())
	, udp_sock_v4(core_->get_io_service())
	, udp_sock_v6(core_->get_io_service())
	, clearpeers(false)
	, peerfindertimer(core_->get_io_service())
	, stun_timer(core_->get_io_service())
	, stun_timer2(core_->get_io_service())
	, stun_server_found(false)
	, stun_retries(0)
{
	bool workv4 = false;
	bool workv6 = false;
	udp_info_v4 = UDPSockInfoRef(new UDPSockInfo(udp_sock_v4, true));
	try {
		udp_sock_v4.open(boost::asio::ip::udp::v4());
		udp_sock_v4.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), UFTT_PORT));

		udp_sock_v4.set_option(boost::asio::ip::udp::socket::broadcast(true));

		start_udp_receive(udp_info_v4, recv_buf_v4, &recv_peer_v4);

		udpsocklist[boost::asio::ip::address_v4::any()] = udp_info_v4;
		udp_info_v4->is_main = true;

		workv4 = true;
	} catch (std::exception& e) {
		std::cout << "Failed to initialise IPv4 UDP socket: " << e.what() << "\n";
	}

	udp_info_v6 = UDPSockInfoRef(new UDPSockInfo(udp_sock_v6, false));
	try {
		udp_sock_v6.open(boost::asio::ip::udp::v6());
		#if defined(__linux__) || defined(__APPLE__)
			udp_sock_v6.set_option(boost::asio::ip::v6_only(true)); // behave more like windows -> less chance for bugs
		#endif
		udp_sock_v6.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v6::any(), UFTT_PORT));

		udp_sock_v6.set_option(boost::asio::ip::udp::socket::broadcast(true));
		udp_sock_v6.set_option(boost::asio::ip::multicast::enable_loopback(true));

		start_udp_receive(udp_info_v6, recv_buf_v6, &recv_peer_v6);

		udpsocklist[boost::asio::ip::address_v6::any()] = udp_info_v6;
		udp_info_v6->is_main = true;

		workv6 = true;
	} catch (std::exception& e) {
		std::cout << "Failed to initialise IPv6 UDP socket: " << e.what() << "\n";
	}

	if (!workv4 && !workv6) {
		core->error_state = 2;
		core->error_string = "Failed to initialize network layer, you probably already have an UFTT instance running.";
	}

	udp_conn_service.reset(new UDPConnService(service, udp_info_v4->sock));

	start_udp_accept(udp_conn_service.get());

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
		tcplistener_v4.open(boost::asio::ip::tcp::v4());
		tcplistener_v4.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), UFTT_PORT));
		tcplistener_v4.listen(16);
		start_tcp_accept(&tcplistener_v4);
	} catch (std::exception& e) {
		std::cout << "Failed to initialize IPv4 TCP listener: " << e.what() << '\n';
	}

	try {
		tcplistener_v6.open(boost::asio::ip::tcp::v6());
		#if defined(__linux__) || defined(__APPLE__)
			tcplistener_v6.set_option(boost::asio::ip::v6_only(true)); // behave more like windows -> less chance for bugs
		#endif
		tcplistener_v6.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v6::any(), UFTT_PORT));
		tcplistener_v6.listen(16);
		start_tcp_accept(&tcplistener_v6);
	} catch (std::exception& e) {
		std::cout << "Failed to initialize IPv6 TCP listener: " << e.what() << '\n';
	}

	// bind autoupdater
	updateProvider.newbuild.connect(
		boost::bind(&SimpleBackend::send_publish, this, uftt_bcst_if, uftt_bcst_ep, _1, 1, true)
	);

	lastpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
	prevpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
	start_peerfinder();

	settings->enablepeerfinder.sigChanged.connect(service.wrap(boost::bind(&SimpleBackend::start_peerfinder, this)));
	settings->nickname.sigChanged.connect(service.wrap(boost::bind(&SimpleBackend::send_publishes, this, uftt_bcst_if, uftt_bcst_ep, 1, true)));
}

SignalConnection SimpleBackend::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, sig_new_share, cb);

}

SignalConnection SimpleBackend::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	return SignalConnection();
	// TODO: implement this
}

SignalConnection SimpleBackend::connectSigNewTask(const boost::function<void(const TaskInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, sig_new_task, cb);
}

SignalConnection SimpleBackend::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, boost::bind(&SimpleBackend::attach_progress_handler, this, tid, cb));
}

void SimpleBackend::doRefreshShares()
{
	service.post(boost::bind(&SimpleBackend::send_query, this, uftt_bcst_if, uftt_bcst_ep));
	service.post(boost::bind(&SimpleBackend::start_peerfinder, this));
}

void SimpleBackend::startDownload(const ShareID& sid, const ext::filesystem::path& path)
{
	service.post(boost::bind(&SimpleBackend::download_share, this, sid, path));
}

void SimpleBackend::doManualPublish(const std::string& host)
{
	try {
		service.post(boost::bind(&SimpleBackend::send_publishes, this,
			uftt_bcst_if, my_endpoint_from_string<boost::asio::ip::udp::endpoint>(host, UFTT_PORT)
			, 1, true
		));
	} catch (std::exception& /*e*/) {}
}

void SimpleBackend::doManualQuery(const std::string& host)
{
	try {
		service.post(boost::bind(&SimpleBackend::send_query, this,
			uftt_bcst_if, my_endpoint_from_string<boost::asio::ip::udp::endpoint>(host, UFTT_PORT)
		));
	} catch (std::exception& /*e*/) {}
}

void SimpleBackend::setModuleID(uint32 mid)
{
	this->mid = mid;
}

void SimpleBackend::notifyAddLocalShare(const LocalShareID& sid)
{
	service.post(boost::bind(&SimpleBackend::add_local_share, this, sid.sid));
}

void SimpleBackend::notifyDelLocalShare(const LocalShareID& sid)
{
	// TODO: implement this
}

// end of interface
// start of internal functions


void SimpleBackend::download_share(const ShareID& sid, const ext::filesystem::path& dlpath)
{
	TaskInfo ret;
	boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
	ret.start_time = now;
	ret.last_progress_report = now;
	ret.status = TASK_STATUS_CONNECTING;

	std::string shareurl = sid.sid;

	ret.shareid = ret.shareinfo.id = sid;
	ret.path = dlpath;
	ret.id.mid = mid;
	ret.id.cid = conlist.size();

	size_t colonpos = shareurl.find_first_of(":");
	std::string proto = shareurl.substr(0, colonpos);
	uint32 version = atoi(proto.substr(6).c_str());
	shareurl.erase(0, proto.size()+3);
	size_t slashpos = shareurl.find_first_of("\\/");
	std::string host = shareurl.substr(0, slashpos);
	std::string share = shareurl.substr(slashpos+1);

	ret.shareinfo.name = platform::makeValidUTF8(share);
	ret.shareinfo.host = host;
	ret.shareinfo.proto = proto;
	ret.isupload = false;

	bool use_udp = proto.substr(0,4) != "uftt";
	boost::asio::ip::tcp::endpoint tcpep;
	my_endpoint_from_string(host, tcpep);
	if (tcpep.port() != UFTT_PORT) use_udp = true;
	if (!tcpep.address().is_v4() && !tcpep.address().to_v6().is_v4_mapped()) use_udp = false;
	if (!use_udp) {
		std::cout << "Connecting(TCP)...\n";
		SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, core));
		conlist.push_back(newconn);
		newconn->taskinfo = ret;

		newconn->getSocket().open(tcpep.protocol());
		newconn->getSocket().async_connect(tcpep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath, version));
	} else {
		std::cout << "Connecting(UDP)...\n";
		UDPSemiConnectionRef newconn(new UDPSemiConnection(service, core, *udp_conn_service));
		conlist.push_back(newconn);
		newconn->taskinfo = ret;

		boost::asio::ip::udp::endpoint udpep(tcpep.address(), tcpep.port());
		newconn->getSocket().async_connect(udpep, boost::bind(&SimpleBackend::dl_connect_handle, this, boost::asio::placeholders::error	, newconn, share, dlpath, version));
	}

	sig_new_task(ret);
}

void SimpleBackend::dl_connect_handle(const boost::system::error_code& e, ConnectionBaseRef conn, std::string name, ext::filesystem::path dlpath, uint32 version)
{
	if (e) {
		std::cout << "connect failed: " << e.message() << '\n';
		conn->taskinfo.error_message = STRFORMAT("Failed to connect: %s", e.message());
		conn->taskinfo.status = TASK_STATUS_ERROR;
		conn->sig_progress(conn->taskinfo);
	} else {
		std::cout << "Connected!\n";
		conn->taskinfo.status = TASK_STATUS_CONNECTED;
		conn->sig_progress(conn->taskinfo);
		conn->handle_tcp_connect(name, dlpath, version);
	}
}

void SimpleBackend::start_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener)
{
	SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, core));
	tcplistener->async_accept(newconn->getSocket(),
		boost::bind(&SimpleBackend::handle_tcp_accept, this, tcplistener, newconn, boost::asio::placeholders::error));
}

void SimpleBackend::handle_tcp_accept(boost::asio::ip::tcp::acceptor* tcplistener, SimpleTCPConnectionRef newconn, const boost::system::error_code& e)
{
	if (e) {
		std::cout << "tcp accept failed: " << e.message() << '\n';
	} else {
		std::cout << "handling tcp accept\n";
		conlist.push_back(newconn);
		boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
		newconn->taskinfo.start_time = now;
		newconn->taskinfo.last_progress_report = now;
		newconn->taskinfo.shareinfo.name = "Upload";
		newconn->taskinfo.id.mid = mid;
		newconn->taskinfo.id.cid = conlist.size()-1;
		newconn->taskinfo.isupload = true;
		newconn->handle_tcp_accept();
		sig_new_task(newconn->taskinfo);
		start_tcp_accept(tcplistener);
	}
}

boost::signals::connection SimpleBackend::attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	boost::signals::connection c;
	int num = tid.cid;
	c = conlist[num]->sig_progress.connect(cb);
	cb(conlist[num]->taskinfo);
	return c;
}

void SimpleBackend::stun_new_addr()
{
	boost::posix_time::ptime checktime = settings->laststuncheck.get() + boost::posix_time::minutes(1);
	settings->laststuncheck = boost::posix_time::second_clock::universal_time();
	stun_timer.cancel();
	stun_timer.expires_at(checktime);
	stun_timer.async_wait(boost::bind(&SimpleBackend::stun_do_check, this, _1, 0));
}

void SimpleBackend::stun_do_check(const boost::system::error_code& e, int retry)
{
	if (e)
		return;

	if (retry == 0) {
		stun_server_found = false;
	} else if (stun_server_found)
		return;

	boost::asio::ip::udp::endpoint stunep(boost::asio::ip::address::from_string("83.103.82.85"), 3478);

	boost::system::error_code err;
	stun_retries = 3;
	send_udp_packet(udp_info_v4, stunep, boost::asio::buffer("\x00\x01\x00\x00         \x0B\x0C     ", 20), err);
	if (retry < 5) {
		++retry;
		asio_timer_oneshot(service, retry * 500, boost::bind(&SimpleBackend::stun_do_check, this, boost::system::error_code(), retry));
	}
}

void SimpleBackend::handle_stun_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len)
{
	if (!recv_peer->address().is_v4() && !recv_peer->address().to_v6().is_v4_mapped()) return;

	uint16 type = (recv_buf[0] << 8) | (recv_buf[1] << 0);
	uint16 slen = (recv_buf[2] << 8) | (recv_buf[3] << 0);
	std::cout << "Got STUN packet\n";

	boost::asio::ip::udp::endpoint mapped_address;
	boost::asio::ip::udp::endpoint source_address;
	boost::asio::ip::udp::endpoint changed_address;

	size_t idx = 20;
	while (idx-20 < len) {
		int atype = 0;
		atype |= (recv_buf[idx++] << 8);
		atype |= (recv_buf[idx++] << 0);
		int tlen = 0;
		tlen |= (recv_buf[idx++] << 8);
		tlen |= (recv_buf[idx++] << 0);

		switch (atype) {
			case 1: {
				mapped_address = boost::asio::ip::udp::endpoint(
					my_addr_from_string(STRFORMAT("%d.%d.%d.%d", (int)recv_buf[idx+4], (int)recv_buf[idx+5], (int)recv_buf[idx+6], (int)recv_buf[idx+7])),
					(recv_buf[idx+2] << 8) | (recv_buf[idx+3] << 0)
				);
				//cout << "MAPPED-ADDRESS: " << mapped_address << '\n';
			}; break;
			case 4: {
				source_address = boost::asio::ip::udp::endpoint(
					my_addr_from_string(STRFORMAT("%d.%d.%d.%d", (int)recv_buf[idx+4], (int)recv_buf[idx+5], (int)recv_buf[idx+6], (int)recv_buf[idx+7])),
					(recv_buf[idx+2] << 8) | (recv_buf[idx+3] << 0)
				);
				//cout << "SOURCE-ADDRESS: " << source_address << '\n';
			}; break;
			case 5: {
				changed_address = boost::asio::ip::udp::endpoint(
					my_addr_from_string(STRFORMAT("%d.%d.%d.%d", (int)recv_buf[idx+4], (int)recv_buf[idx+5], (int)recv_buf[idx+6], (int)recv_buf[idx+7])),
					(recv_buf[idx+2] << 8) | (recv_buf[idx+3] << 0)
				);
				//cout << "CHANGED-ADDRESS: " << changed_address << '\n';
			}; break;
			//default: cout << "type: " << type << '\n';
		}
		idx += tlen;
	}

	if (type == 0x0101) { // binding response
		if (source_address == changed_address) {
			stun_server_found = true;
			cout << "Internet reachable address: " << mapped_address << '\n';
			if (mapped_address != stun_endpoint) {
				stun_endpoint = mapped_address;
				lastpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
				prevpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
				start_peerfinder();
			}
			stun_timer2.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::minutes(1));
			stun_timer2.async_wait(boost::bind(&SimpleBackend::stun_send_bind, this, _1));
		} else if (stun_retries > 0) {
			--stun_retries;
			stun_send_bind(boost::system::error_code());
		}
	}
}

void SimpleBackend::stun_send_bind(const boost::system::error_code& e)
{
	if (e)
		return;

	boost::asio::ip::udp::endpoint stunep(boost::asio::ip::address::from_string("83.103.82.85"), 3478);
	boost::system::error_code err;
	send_udp_packet(udp_info_v4, stunep, boost::asio::buffer("\x00\x01\x00\x08         \x0B\x0C     \x00\x03\x00\x04\x00\x00\x00\x06", 28), err);

	stun_timer2.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::minutes(10));
	stun_timer2.async_wait(boost::bind(&SimpleBackend::stun_send_bind, this, _1));
}

void SimpleBackend::start_udp_accept(UDPConnService* cservice)
{
	UDPSemiConnectionRef newconn(new UDPSemiConnection(service, core, *cservice));
	newconn->getSocket().async_accept(
		boost::bind(&SimpleBackend::handle_udp_accept, this, cservice, newconn, boost::asio::placeholders::error)
	);
}

void SimpleBackend::handle_udp_accept(UDPConnService* cservice, UDPSemiConnectionRef newconn, const boost::system::error_code& e)
{
	if (e) {
		std::cout << "udp accept failed: " << e.message() << '\n';
	} else {
		std::cout << "handling udp accept\n";
		conlist.push_back(newconn);
		newconn->taskinfo.shareinfo.name = "< N/A >";
		newconn->taskinfo.id.mid = mid;
		newconn->taskinfo.id.cid = conlist.size()-1;
		newconn->taskinfo.isupload = true;
		newconn->handle_tcp_accept();
		sig_new_task(newconn->taskinfo);
	}
	start_udp_accept(cservice);
}

void SimpleBackend::handle_peerfinder_query(const boost::system::error_code& e, boost::shared_ptr<boost::asio::http_request> request, int prog)
{
	if (prog >= 0)
		return;
	if (e) {
		std::cout << "Failed to get simple global peer list: " << e.message() << '\n';
	} else {
		try {
			const std::vector<uint8>& page = request->getContent();

			uint8 content_start[] = "*S*T*A*R*T*\r";
			uint8 content_end[] = "*S*T*O*P*\r";

			typedef std::vector<uint8>::const_iterator vpos;

			vpos spos = std::search(page.begin(), page.end(), content_start   , content_start    + sizeof(content_start   ) - 1);
			vpos epos = std::search(page.begin(), page.end(), content_end     , content_end      + sizeof(content_end     ) - 1);

			if (spos == page.end() || epos == page.end())
				throw std::runtime_error("No valid start/end markers found.");

			spos += sizeof(content_start) - 1;

			std::string content(spos, epos);

			std::vector<std::string> lines;
			boost::split(lines, content, boost::is_any_of("\r\n"));

			std::set<boost::asio::ip::address> addrs;

			if (clearpeers) {
				trypeers.clear();
				clearpeers = false;
			}

			BOOST_FOREACH(const std::string& line, lines) {
				std::vector<std::string> cols;
				boost::split(cols, line, boost::is_any_of("\t"));
				if (cols.size() == 2) {
					trypeers.insert(boost::asio::ip::udp::endpoint(my_addr_from_string(cols[0]), atoi(cols[1].c_str())));
				}
			}
		} catch (std::exception& ex) {
			std::cout << "Error parsing global peer list: " << ex.what() << '\n';
		}
	}

	start_peerfinder();
}

void SimpleBackend::handle_peerfinder_timer(const boost::system::error_code& e)
{
	if (e) {
		std::cout << "handle_peerfinder_timer: " << e.message() << '\n';
	} else {
		if (settings->enablepeerfinder) {
			prevpeerquery = lastpeerquery;
			lastpeerquery = boost::posix_time::second_clock::universal_time();

			clearpeers = true; // new round of request, clear trypeers once
			std::vector<std::string> urlbases;
			//urlbases.push_back("http://hackykid.heliohost.org/site/bootstrap.php");
			urlbases.push_back("http://hackykid.awardspace.com/site/bootstrap.php");
			urlbases.push_back("http://servertje.info.tm:40080/uftt/bootstrap.php");
			BOOST_FOREACH(std::string url, urlbases) {
				url += "?reg=1&type=simple&class=1wdvhi09ehvnazmq23jd";
				url += STRFORMAT("&ip=%s", stun_endpoint.address());
				url += STRFORMAT("&port=%d", stun_endpoint.port());
				boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service));
				request->async_http_request(url, boost::bind(&SimpleBackend::handle_peerfinder_query, this, _2, request, _1));
			}
		}
	}
}

void SimpleBackend::start_peerfinder()
{
	if (!settings->enablepeerfinder) return;

	boost::posix_time::ptime dl;
	if (lastpeerquery - prevpeerquery > boost::posix_time::minutes(55))
		dl = lastpeerquery + boost::posix_time::seconds(20);
	else
		dl = lastpeerquery + boost::posix_time::minutes(50);

	peerfindertimer.cancel();
	peerfindertimer.expires_at(dl);
	peerfindertimer.async_wait(boost::bind(&SimpleBackend::handle_peerfinder_timer, this, _1));

	std::cout << "Peer check at: " << my_datetime_to_string(dl) << '\n';

	const uint8 udp_send_buf[5] = {
		(1 >>  0) & 0xff,
		(1 >>  8) & 0xff,
		(1 >> 16) & 0xff,
		(1 >> 24) & 0xff,
		UDPPACK_QUERY_PEER
	};

	std::cout << "Finding peers...\n";
	boost::system::error_code err;
	BOOST_FOREACH(const boost::asio::ip::udp::endpoint& ep, trypeers) {
		try {
			send_udp_packet(udp_info_v4, ep, boost::asio::buffer(udp_send_buf), err);
		} catch (...) {
		}
	}
	std::cout << "Finding peers...Done\n";
}

void SimpleBackend::start_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer)
{
	si->sock.async_receive_from(boost::asio::buffer(recv_buf, UDP_BUF_SIZE), *recv_peer,
		boost::bind(&SimpleBackend::handle_udp_receive, this, si, recv_buf, recv_peer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
	);
}

std::set<uint32> SimpleBackend::parseVersions(uint8* base, uint start, uint len)
{
	uint end;
	return parseVersions(base, start, len, &end);
}

std::set<uint32> SimpleBackend::parseVersions(uint8* base, uint start, uint len, uint* end)
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

void SimpleBackend::handle_discovery_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len, uint32 version)
{
	//std::cout << "got packet type " << (int)recv_buf[4] << " from " << *recv_peer << "\n";
	switch (recv_buf[4]) {
		case UDPPACK_QUERY_SHARE: if (version == 1) { // type = broadcast;
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
		case UDPPACK_REPLY_SHARE: { // type = reply;
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
						std::string surl = STRFORMAT("uftt-v%d://%s/%s", sver, *recv_peer, sname);
						ShareInfo sinfo;
						sinfo.name = platform::makeValidUTF8(sname);
						sinfo.proto = STRFORMAT("uftt-v%d", sver);
						sinfo.host = STRFORMAT("%s", *recv_peer);
						sinfo.id.sid = surl;
						sinfo.id.mid = mid;
						if (versions.count(3)) { // Version 3 added support for usernames (nicknames)
							int nickname_length = recv_buf[slen + 6 + vlen];
							std::string nickname((char*)recv_buf + 6 + slen + 1 + vlen, (char*)recv_buf + 6 + slen + 1 + vlen + nickname_length);
							sinfo.user = nickname;
						}
						sig_new_share(sinfo);
					}
				}
			}
		}; break;
		case UDPPACK_REPLY_UPDATE: // type = autoupdate share
		case UDPPACK_REPLY_UPDATE_NEW: { // type = futureproof autoupdate share
			if (len >= 6 && (version == 1)) {
				uint32 slen = recv_buf[5];
				if (len >= slen+6) {
					std::string sname((char*)recv_buf+6, (char*)recv_buf+6+slen);
					std::string surl = STRFORMAT("uftt-v%d://%s/%s/%s", 2, *recv_peer, AutoUpdaterTag, sname);;
					ShareInfo sinfo;
					sinfo.name = platform::makeValidUTF8(sname);
					sinfo.proto = STRFORMAT("uftt-v%d", 2); // from now on autoupdates use the new protocol
					sinfo.host = STRFORMAT("%s", *recv_peer);
					sinfo.id.sid = surl;
					sinfo.id.mid = mid;
					sinfo.isupdate = true;
					sig_new_share(sinfo);
				}
			}
		}; break;
		case UDPPACK_QUERY_PEER: { // peerfinder query
			boost::system::error_code err;
			const uint8 udp_send_buf[5] = {
				(1 >>  0) & 0xff,
				(1 >>  8) & 0xff,
				(1 >> 16) & 0xff,
				(1 >> 24) & 0xff,
				UDPPACK_REPLY_PEER
			};
			send_udp_packet(si, *recv_peer, boost::asio::buffer(udp_send_buf), err);
		};// intentional fallthrough
		case UDPPACK_REPLY_PEER: { // peerfinder reply
			foundpeers.insert(*recv_peer);
			send_query(si, *recv_peer);
		}; break;
	}
}

void SimpleBackend::handle_rudp_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len)
{
	if (recv_peer->address().is_v4() || recv_peer->address().to_v6().is_v4_mapped())
		udp_conn_service->handle_read(recv_buf, len, recv_peer);
}

void SimpleBackend::handle_udp_receive(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, const boost::system::error_code& e, std::size_t len)
{
	if (!e) {
		if (udpfails > 0) udpfails = 0;
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
		++udpfails;
	}

	if (!e || udpfails < 512) {
		start_udp_receive(si, recv_buf, recv_peer);
	} else {
		std::cout << "retry limit reached, giving up on receiving udp packets temprarily\n";
		udpfails -= 128;
		asio_timer_oneshot(service, (int)1000, boost::bind(&SimpleBackend::start_udp_receive, this, si, recv_buf, recv_peer));
	}
}

void SimpleBackend::send_udp_packet_to(boost::asio::ip::udp::socket& sock, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags)
{
	if (udpfails > -512) --udpfails;
	sock.send_to(
		buf,
		ep,
		flags,
		err);
}

void SimpleBackend::send_udp_packet_to(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags)
{
	//std::cout << "Send udp packet to " << ep;
	//if (boost::asio::buffer_size(buf) >= 5)
	//	std::cout << " = " << (int)boost::asio::buffer_cast<const uint8*>(buf)[4];
	//std::cout << " (" << si->is_v4 << ")\n";
	if (si->is_v4 == ep.address().is_v4())
		send_udp_packet_to(si->sock, ep, buf, err, flags);
	//else
	//	std::cout << "Skipped\n";
}

void SimpleBackend::send_udp_packet(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, boost::asio::const_buffers_1 buf, boost::system::error_code& err, int flags)
{
	if (!si) { // == uftt_bcst_if
		typedef std::pair<boost::asio::ip::address, UDPSockInfoRef> sipair;
		BOOST_FOREACH(sipair sip, udpsocklist) if (sip.second) {
			send_udp_packet(sip.second, ep, buf, err, flags);
			if (err)
				std::cout << "send to (" << ep << ") failed: " << err.message() << '\n';
		}
	} else {
		if (ep == uftt_bcst_ep) {
			if (!si->is_main)
				send_udp_packet_to(si, si->bcst_ep, buf, err, flags);
			else
				BOOST_FOREACH(const boost::asio::ip::udp::endpoint& fpep, foundpeers)
					send_udp_packet_to(si, fpep, buf, err, flags);
		} else
			send_udp_packet_to(si, ep, buf, err, flags);
	}
}

void SimpleBackend::send_query(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep)
{
	uint8 udp_send_buf[1024];
	udp_send_buf[ 0] = 0x01;
	udp_send_buf[ 1] = 0x00;
	udp_send_buf[ 2] = 0x00;
	udp_send_buf[ 3] = 0x00;
	udp_send_buf[ 4] = UDPPACK_QUERY_SHARE;
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
	send_udp_packet(si, ep, boost::asio::buffer((const uint8*)udp_send_buf, plen), err);
	if (err)
		std::cout << "qeury to '" << ep << "' failed: " << err.message() << '\n';
}

/**
 * send_publish announces our list of shares to the network.
 * Note, there is a trick implemented here to also support older versions of the protocol.
 * The trick is that we reuse the version number 1. This is because in version 1 uftt did
 * not include backward/forward version info. So we send the packet as version 1 and add
 * the extended info at the end. Old versions will ignore the extra data while new versions
 * will parse this and everything works properly
 **/
void SimpleBackend::send_publish(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, const std::string& name, int sharever, bool isbuild)
{
	uint8 udp_send_buf[1024];
	udp_send_buf[0] = (sharever >>  0) & 0xff;
	udp_send_buf[1] = (sharever >>  8) & 0xff;
	udp_send_buf[2] = (sharever >> 16) & 0xff;
	udp_send_buf[3] = (sharever >> 24) & 0xff;
	udp_send_buf[4] = isbuild ? UDPPACK_REPLY_UPDATE : UDPPACK_REPLY_SHARE;
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

	if (sharever == 1 || sharever >= 3) { // Version 3 added support for usernames (nicknames)
		#undef min
		std::string nickname = settings->nickname;
		int nickname_length = std::min((size_t)255, nickname.size());
		udp_send_buf[plen++] = nickname_length;
		memcpy(&udp_send_buf[plen], nickname.data(), nickname_length);
		plen += nickname_length;
	}

	boost::system::error_code err;
	send_udp_packet(si, ep, boost::asio::buffer((const uint8*)udp_send_buf, plen), err);
	if (err)
		std::cout << "publish of '" << name << "' failed: " << err.message() << '\n';
}

void SimpleBackend::send_publishes(UDPSockInfoRef si, const boost::asio::ip::udp::endpoint& ep, int sharever, bool sendbuilds)
{
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

void SimpleBackend::add_local_share(std::string name)
{
	send_publish(uftt_bcst_if, uftt_bcst_ep, name, 1);
}

void SimpleBackend::new_iface(UDPSockInfoRef si)
{
	send_publishes(si, uftt_bcst_ep, 1, true);
	send_query    (si, uftt_bcst_ep);

	asio_timer_oneshot(service, (int)1000, boost::bind(&SimpleBackend::send_publishes, this, si, uftt_bcst_ep, 1, true));
	asio_timer_oneshot(service, (int)1000, boost::bind(&SimpleBackend::send_query    , this, si, uftt_bcst_ep));
}

void SimpleBackend::add_ip_addr_ipv4(std::pair<boost::asio::ip::address, boost::asio::ip::address> addrwbcst)
{
	boost::asio::ip::address addr = addrwbcst.first;
	boost::asio::ip::address bcaddr = addrwbcst.second;
	if (!udpsocklist[addr]) {
		UDPSockInfoRef newsock(new UDPSockInfo(udp_info_v4->sock, true));
		newsock->bcst_ep = boost::asio::ip::udp::endpoint(bcaddr, UFTT_PORT);
		udpsocklist[addr] = newsock;
		new_iface(newsock);
		stun_new_addr();
	} else
		std::cout << "Duplicate added adress: " << addr << '\n';
}

void SimpleBackend::add_ip_addr_ipv6(boost::asio::ip::address addr) {
	if (!udpsocklist[addr]) {
		try {
			UDPSockInfoRef newsock(new UDPSockInfo(udp_info_v6->sock, false));
			boost::asio::ip::address_v6 bcaddr = my_addr_from_string("ff02::1").to_v6();
			bcaddr.scope_id(addr.to_v6().scope_id());

			try {
				newsock->sock.set_option(boost::asio::ip::multicast::join_group(bcaddr, bcaddr.scope_id()));
			} catch (boost::system::system_error& e) {
				#ifdef WIN32
					if (e.code().value() == WSAEINVAL)
						std::cout << "Warning: failed to join multicast group for " << addr << '\n';
					else
				#endif
				#if defined(__linux__) || defined(__APPLE__)
					if (e.code().value() == EADDRINUSE)
						std::cout << "Warning: failed to join multicast group for " << addr << '\n';
					else
				#endif
					throw e;
			}

			newsock->bcst_ep = boost::asio::ip::udp::endpoint(bcaddr, UFTT_PORT);

			udpsocklist[addr] = newsock;

			new_iface(newsock);
		} catch (std::exception& e) {
			std::cout << "Failed to initialise IPv6 address (" << addr << ") : " << e.what() << '\n';
		}
	} else
		std::cout << "Duplicate added adress: " << addr << '\n';
}

void SimpleBackend::del_ip_addr(boost::asio::ip::address addr) {
	udpsocklist.erase(addr);
}
