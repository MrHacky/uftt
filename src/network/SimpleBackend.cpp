#include "SimpleBackend.h"

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(SimpleBackend);

#include <set>

#include "SimpleTCPConnection.h"

using namespace std;

void SimpleBackend::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	sig_new_share.connect(cb);
}

void SimpleBackend::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	// TODO: implement this
}

void SimpleBackend::connectSigNewTask(const boost::function<void(const TaskInfo&)>& cb)
{
	sig_new_task.connect(cb);
}

void SimpleBackend::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	service.post(boost::bind(&SimpleBackend::attach_progress_handler, this, tid, cb));
}

void SimpleBackend::doRefreshShares()
{
	service.post(boost::bind(&SimpleBackend::send_query, this, uftt_bcst_if, uftt_bcst_ep));
}

void SimpleBackend::startDownload(const ShareID& sid, const boost::filesystem::path& path)
{
	service.post(boost::bind(&SimpleBackend::download_share, this, sid, path));
}

void SimpleBackend::doManualPublish(const std::string& host)
{
	try {
		service.post(boost::bind(&SimpleBackend::send_publishes, this,
			uftt_bcst_if, boost::asio::ip::udp::endpoint(my_addr_from_string(host), UFTT_PORT)
			, true, true
		));
	} catch (std::exception& /*e*/) {}
}

void SimpleBackend::doManualQuery(const std::string& host)
{
	try {
		service.post(boost::bind(&SimpleBackend::send_query, this,
			uftt_bcst_if, boost::asio::ip::udp::endpoint(my_addr_from_string(host), UFTT_PORT)
		));
	} catch (std::exception& /*e*/) {}
}

void SimpleBackend::doSetPeerfinderEnabled(bool enabled)
{
	service.post(boost::bind(&SimpleBackend::start_peerfinder, this, enabled));
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


void SimpleBackend::download_share(const ShareID& sid, const boost::filesystem::path& dlpath)
{
	TaskInfo ret;
	std::string shareurl = sid.sid;

	ret.shareid = ret.shareinfo.id = sid;
	ret.id.mid = mid;
	ret.id.cid = conlist.size();

	size_t colonpos = shareurl.find_first_of(":");
	std::string proto = shareurl.substr(0, colonpos);
	uint32 version = atoi(proto.substr(6).c_str());
	shareurl.erase(0, proto.size()+3);
	size_t slashpos = shareurl.find_first_of("\\/");
	std::string host = shareurl.substr(0, slashpos);
	std::string share = shareurl.substr(slashpos+1);
	SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, core));
	//newconn->sig_progress.connect(handler);
	conlist.push_back(newconn);

	ret.shareinfo.name = share;
	ret.shareinfo.host = host;
	ret.shareinfo.proto = proto;
	ret.isupload = false;

	newconn->taskinfo = ret;

	boost::asio::ip::tcp::endpoint ep(my_addr_from_string(host), UFTT_PORT);
	newconn->getSocket().open(ep.protocol());
	std::cout << "Connecting...\n";
	newconn->getSocket().async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath, version));

	sig_new_task(ret);
}

void SimpleBackend::dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath, uint32 version)
{
	if (e) {
		std::cout << "connect failed: " << e.message() << '\n';
	} else {
		std::cout << "Connected!\n";
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
		newconn->taskinfo.shareinfo.name = "Upload";
		newconn->taskinfo.id.mid = mid;
		newconn->taskinfo.id.cid = conlist.size()-1;
		newconn->taskinfo.isupload = true;
		newconn->handle_tcp_accept();
		sig_new_task(newconn->taskinfo);
		start_tcp_accept(tcplistener);
	}
}

void SimpleBackend::attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	int num = tid.cid;
	conlist[num]->sig_progress.connect(cb);
	conlist[num]->sig_progress(conlist[num]->taskinfo);
}

void SimpleBackend::stun_new_addr()
{
	boost::posix_time::ptime checktime = settings.laststuncheck + boost::posix_time::minutes(1);
	stun_timer.expires_at(checktime);
	stun_timer.async_wait(boost::bind(&SimpleBackend::stun_do_check, this, _1, 0));
}

void SimpleBackend::stun_do_check(const boost::system::error_code& e, int retry)
{
	if (e)
		return;

	if (retry == 0) {
		settings.laststuncheck = boost::posix_time::second_clock::universal_time();
		stun_server_found = false;
	} else if (stun_server_found)
		return;

	boost::asio::ip::udp::endpoint stunep(boost::asio::ip::address::from_string("83.103.82.85"), 3478);

	boost::system::error_code err;
	send_udp_packet(udp_info_v4, stunep, boost::asio::buffer("\x00\x01\x00\x00         \x0B\x0C     ", 20), err);
	if (retry < 5) {
		++retry;
		asio_timer_oneshot(service, retry * 500, boost::bind(&SimpleBackend::stun_do_check, this, boost::system::error_code(), retry));
	}
}

void SimpleBackend::handle_stun_packet(UDPSockInfoRef si, uint8* recv_buf, boost::asio::ip::udp::endpoint* recv_peer, std::size_t len)
{
	if (!recv_peer->address().is_v4()) return;

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
				settings.lastpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
				settings.prevpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
				start_peerfinder();
			}
			stun_timer2.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::minutes(1));
			stun_timer2.async_wait(boost::bind(&SimpleBackend::stun_send_bind, this, _1));
		} else {
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

	stun_timer2.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::minutes(1));
	stun_timer2.async_wait(boost::bind(&SimpleBackend::stun_send_bind, this, _1));
}
