#include "SimpleBackend.h"

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(SimpleBackend);

#include <set>

#include "SimpleConnection.h"

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
	} catch (std::exception& e) {}
}

void SimpleBackend::doManualQuery(const std::string& host)
{
	try {
		service.post(boost::bind(&SimpleBackend::send_query, this,
			uftt_bcst_if, boost::asio::ip::udp::endpoint(my_addr_from_string(host), UFTT_PORT)
		));
	} catch (std::exception& e) {}
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

	newconn->shareid = sid;

	boost::asio::ip::tcp::endpoint ep(my_addr_from_string(host), UFTT_PORT);
	newconn->getSocket().open(ep.protocol());
	std::cout << "Connecting...\n";
	newconn->getSocket().async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath, version));

	ret.shareinfo.name = share;
	ret.shareinfo.host = host;
	ret.shareinfo.proto = proto;
	ret.isupload = false;
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
		newconn->handle_tcp_accept();
		TaskInfo tinfo;
		tinfo.shareinfo.name = "Upload";
		tinfo.id.mid = mid;
		tinfo.id.cid = conlist.size()-1;
		tinfo.isupload = true;
		sig_new_task(tinfo);
		start_tcp_accept(tcplistener);
	}
}

void SimpleBackend::attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	// TODO: fix this
	int num = tid.cid;
	conlist[num]->sig_progress.connect(cb);
}
