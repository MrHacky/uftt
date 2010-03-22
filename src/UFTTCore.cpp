#include "UFTTCore.h"

#include "network/INetModule.h"
#include "network/NetModuleLinker.h"

#include "util/Filesystem.h"
#include "util/StrFormat.h"

#include <boost/foreach.hpp>

typedef boost::shared_ptr<INetModule> INetModuleRef;

namespace {
	std::string getstr0(boost::asio::ip::tcp::socket& sock)
	{
		std::string r;
		char ret;
		while (true) {
			boost::asio::read(sock, boost::asio::buffer(&ret, 1));
			if (ret == 0) return r;
			r.push_back(ret);
		}
	}
}

UFTTCore::UFTTCore(UFTTSettingsRef settings_, int argc, char **argv)
: settings(settings_)
, disk_service(io_service)
, local_listener(io_service)
, error_state(0)
{
	localshares.clear();

	boost::asio::ip::tcp::endpoint local_endpoint(boost::asio::ip::address_v4::loopback(), UFTT_PORT-1);
	try {
		local_listener.open(boost::asio::ip::tcp::v4());
		local_listener.bind(local_endpoint);
		local_listener.listen(16);

		handle_local_connection(boost::shared_ptr<boost::asio::ip::tcp::socket>(), boost::system::error_code());
	} catch (std::exception& e) {
		bool success = false;
		try {
			// failed to open listening socket, try to connect to existing uftt process
			boost::asio::ip::tcp::socket sock(io_service);
			sock.open(boost::asio::ip::tcp::v4());
			sock.connect(local_endpoint);
			boost::asio::write(sock, boost::asio::buffer(STRFORMAT("args%c%d%c", (char)0, argc, (char)0)));
			for (size_t i = 0; i < argc; ++i)
				boost::asio::write(sock, boost::asio::buffer(STRFORMAT("%s%c", argv[i], (char)0)));

			uint8 ret;
			boost::asio::read(sock, boost::asio::buffer(&ret, 1));
			if (ret != 0) std::runtime_error("Read failed");
			std::string wid = getstr0(sock);
			platform::activateWindow(wid);
			success = true;
		} catch (std::exception& e) {
			std::cout << "Failed to listen and failed to connect" << e.what() << "\n";
		}
		if (success) throw 0;
	}

	netmodules = NetModuleLinker::getNetModuleList(this);
	for (uint i = 0; i < netmodules.size(); ++i)
		netmodules[i]->setModuleID(i);

	std::vector<std::string> v;
	for(int i = 0; i < argc; ++i)
		v.push_back(argv[i]);

	handle_args(v, false);

	servicerunner = boost::thread(boost::bind(&UFTTCore::servicerunfunc, this)).move();
}

void UFTTCore::handle_local_connection(boost::shared_ptr<boost::asio::ip::tcp::socket> sock, const boost::system::error_code& e)
{
	if (e) {
		std::cout << "handle_local_connection: " << e << "\n";
	}

	if (!e) {
		boost::shared_ptr<boost::asio::ip::tcp::socket> newsock(new boost::asio::ip::tcp::socket(io_service));
		local_listener.async_accept(*newsock,
			boost::bind(&UFTTCore::handle_local_connection, this, newsock, boost::asio::placeholders::error));
	}

	if (sock && !e) {
		std::string s;
		std::vector<std::string> v;
		try {
			std::string r = getstr0(*sock);
			if (r != "args") return;
			size_t n = boost::lexical_cast<size_t>(getstr0(*sock));
			for (size_t i = 0; i < n; ++i)
				v.push_back(getstr0(*sock));
			uint8 st = 0;
			boost::asio::write(*sock, boost::asio::buffer(&st, 1));
			boost::asio::write(*sock, boost::asio::buffer(STRFORMAT("%s%c", mwid, (char)0)));

			boost::asio::read(*sock, boost::asio::buffer(&st, 1)); // expected to fail with EOF
		} catch (std::exception& ex) {
			std::cout << "handle_local_connection: " << ex.what() << "\n";
		}
		if (!v.empty())
			handle_args(v, true);
	}
}

void UFTTCore::setMainWindowId(const std::string& mwid_)
{
	mwid = mwid_;
}

void UFTTCore::handle_args(const std::vector<std::string>& args, bool fromremote)
{
	for (size_t i = 1; i < args.size(); ++i) {
		boost::filesystem::path fp(args[i]);
		if (!ext::filesystem::exists(fp)) break;
		if (fp.leaf() == ".") fp.remove_leaf(); // linux thingy
		addLocalShare(fp.leaf(), fp);
	}

	if (fromremote)
		sigGuiCommand(GUI_CMD_SHOW);
}

UFTTCore::~UFTTCore()
{
	disk_service.stop();
	io_service.stop();
	servicerunner.join();
}

void UFTTCore::servicerunfunc() {
	boost::asio::io_service::work wobj(io_service);
	io_service.run();
}

void UFTTCore::connectSigGuiCommand(const boost::function<void(GuiCommand)>& cb)
{
	sigGuiCommand.connect(cb);
}

// Local Share management
void UFTTCore::addLocalShare(const std::string& name, const boost::filesystem::path& path)
{
	// TODO: locking
	localshares[name] = path;

	LocalShareID sid;
	sid.sid = name;

	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->notifyAddLocalShare(sid);
}

void UFTTCore::delLocalShare(const LocalShareID& id)
{
	// TODO: locking
	localshares[id.sid] = "";

	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->notifyDelLocalShare(id);
}

void UFTTCore::connectSigAddLocalShare(const LocalShareInfo& info)
{
	// TODO: implement this
}

void UFTTCore::connectSigDelLocalShare(const LocalShareID& id)
{
	// TODO: implement this
}

boost::filesystem::path UFTTCore::getLocalSharePath(const LocalShareID& id)
{
	return getLocalSharePath(id.sid);
}

boost::filesystem::path UFTTCore::getLocalSharePath(const std::string& id)
{
	// TODO: locking
	return localshares[id];
}

std::vector<std::string> UFTTCore::getLocalShares()
{
	std::vector<std::string> result;

	typedef std::pair<const std::string, boost::filesystem::path> tpair;
	BOOST_FOREACH(tpair& p, localshares)
		if (!p.second.empty())
			result.push_back(p.first);

	return result;
}

// Remote Share Listing/Downloading
void UFTTCore::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->connectSigAddShare(cb);
}

void UFTTCore::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->connectSigDelShare(cb);
}

void UFTTCore::connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>& cb)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->connectSigNewTask(cb);
}

void UFTTCore::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	if (tid.mid < netmodules.size())
		netmodules[tid.mid]->connectSigTaskStatus(tid, cb);
}

void UFTTCore::startDownload(const ShareID& sid, const boost::filesystem::path& path)
{
	if (sid.mid < netmodules.size())
		netmodules[sid.mid]->startDownload(sid, path);
}


// Misc network functions
void UFTTCore::doManualPublish(const std::string& host)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->doManualPublish(host);
}

void UFTTCore::doManualQuery(const std::string& host)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->doManualQuery(host);
}

void UFTTCore::doRefreshShares()
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->doRefreshShares();
}


// Deprecated but still in use
UFTTSettingsRef UFTTCore::getSettingsRef()
{
	return settings;
}

// Service getters
boost::asio::io_service& UFTTCore::get_io_service()
{
	return io_service;
}

services::diskio_service& UFTTCore::get_disk_service()
{
	return disk_service;
}
