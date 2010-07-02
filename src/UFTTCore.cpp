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
	args = platform::getUTF8CommandLine(argc, argv);

	boost::asio::ip::tcp::endpoint local_endpoint(boost::asio::ip::address_v4::loopback(), UFTT_PORT-1);
	try {
		local_listener.open(boost::asio::ip::tcp::v4());
		local_listener.bind(local_endpoint);
		local_listener.listen(16);

		handle_local_connection(boost::shared_ptr<boost::asio::ip::tcp::socket>(), boost::system::error_code());
	} catch (std::exception& /*e*/) {
		bool success = false;
		try {
			// failed to open listening socket, try to connect to existing uftt process
			std::string file_list = STRFORMAT("argv[0]%c", (char)0);
			size_t      file_count = 1;
			for (size_t i = 1; i < args.size(); ++i) {
				ext::filesystem::path p(
					boost::filesystem::system_complete(ext::filesystem::path(args[i]))
				);
				if (ext::filesystem::exists(p)) {
					file_list += STRFORMAT("%s%c", p.string(), (char)0);
					++file_count;
				} else {
					std::cout << STRFORMAT("%s: %s: No such file or directory.", args[0], args[i]) << std::endl;
				}
			}
			boost::asio::ip::tcp::socket sock(io_service);
			sock.open(boost::asio::ip::tcp::v4());
			sock.connect(local_endpoint);
			boost::asio::write(sock, boost::asio::buffer(STRFORMAT("args%c%d%c", (char)0, file_count, (char)0)));
			boost::asio::write(sock, boost::asio::buffer(file_list));
			uint8 ret;
			boost::asio::read(sock, boost::asio::buffer(&ret, 1));
			if (ret != 0) std::runtime_error("Read failed");
			std::string wid = getstr0(sock);
			platform::activateWindow(wid);
			success = true;
		} catch (std::exception& e) {
			std::cout << "Failed to listen and failed to connect: " << e.what() << std::endl;
		}
		if (success) throw 0;
	}
}

void UFTTCore::initialize()
{
	localshares.clear();

	netmodules = NetModuleLinker::getNetModuleList(this);
	for (uint i = 0; i < netmodules.size(); ++i)
		netmodules[i]->setModuleID(i);

	handle_args(args, false);
	args.clear();

	servicerunner = boost::thread(boost::bind(&UFTTCore::servicerunfunc, this)).move();

	settings->uftt_send_to.connectChanged(boost::bind(&platform::setSendToUFTTEnabled, _1), true);
	settings->uftt_desktop_shortcut.connectChanged(boost::bind(&platform::setDesktopShortcutEnabled, _1), true);
	settings->uftt_quicklaunch_shortcut.connectChanged(boost::bind(&platform::setQuicklaunchShortcutEnabled, _1), true);
	settings->uftt_startmenu_group.connectChanged(boost::bind(&platform::setStartmenuGroupEnabled, _1), true);
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
		ext::filesystem::path fp(args[i]);
		if (!ext::filesystem::exists(fp)) break;
		addLocalShare(fp);
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
void UFTTCore::addLocalShare(const std::string& name, const ext::filesystem::path& path)
{
	// TODO: locking
	localshares[name] = path;

	LocalShareID sid;
	sid.sid = name;

	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->notifyAddLocalShare(sid);
}

void UFTTCore::addLocalShare(const ext::filesystem::path& path)
{
	ext::filesystem::path p = path;
	while (p.leaf() == ".") p.remove_leaf(); // linux thingy
	std::string name = p.leaf();
#ifdef WIN32
	if (name == "/" && p.string().size() == 3 && p.string()[1] == ':')
		name = p.string()[0];
#else
	if (name == "/" && p.string() == "/")
		name = "[root]";
#endif
	addLocalShare(name, p);
}

/**
 * Checks whether name is a  local share (shared by this UFTT instance)
 * @param name is a std::string containing the name of a share (ShareInfo.name)
 * @return (\\exists i : i \\in localshares : i.name == name)
 */
bool UFTTCore::isLocalShare(const std::string& name) {
	// TODO: locking
	return localshares.count(name) > 0;
}

/**
 * Returns the LocalShareID of name, iff isLocalShare(name)
 * @param name is a std::string containing the name of a share (ShareInfo.name)
 * @param id is a pointer to a LocalShareID which will be filled with the
 *        LocalShareID of name.
 * @return true iff a local share with name exists, and id will contain the
 *         LocalShareID for the share with the given name
 * @todo Decide if there can be multiple local shares with the same name, but
 *       having different LocalShareIDs.
 */
bool UFTTCore::getLocalShareID(const std::string& name, LocalShareID* id) {
	if(isLocalShare(name)) {
		id->sid = name;
		return true;
	}
	return false;
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

ext::filesystem::path UFTTCore::getLocalSharePath(const LocalShareID& id)
{
	return getLocalSharePath(id.sid);
}

ext::filesystem::path UFTTCore::getLocalSharePath(const std::string& id)
{
	// TODO: locking
	return localshares[id];
}

std::vector<std::string> UFTTCore::getLocalShares()
{
	std::vector<std::string> result;

	typedef std::pair<const std::string, ext::filesystem::path> tpair;
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

void UFTTCore::startDownload(const ShareID& sid, const ext::filesystem::path& path)
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
