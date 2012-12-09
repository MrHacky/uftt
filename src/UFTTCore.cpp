#include "UFTTCore.h"

#include "network/INetModule.h"
#include "network/NetModuleLinker.h"

#include "util/Coro.h"
#include "util/Filesystem.h"
#include "util/StrFormat.h"

#include <boost/foreach.hpp>

#include "AutoUpdate.h"
#include "Globals.h"
#include "BuildString.h"

typedef boost::shared_ptr<INetModule> INetModuleRef;


struct UFTTCore::CommandExecuteHelper: public ext::coro::base<CommandExecuteHelper> {
	UFTTCore* core;
	CommandLineInfo* cmdinfo;
	boost::function<void(void)> cb;

	CommandLineCommand clc;
	size_t icmd;

	SignalConnection sigconn;

	// --download-share
	std::string share;
	TaskInfo taskinfo;

	// --notify-socket
	boost::asio::ip::tcp::socket sock;

	CommandExecuteHelper(UFTTCore* core_, CommandLineInfo* cmdinfo_, const boost::function<void(void)>& cb_)
		: core(core_), cmdinfo(cmdinfo_), cb(cb_), sock(core->io_service)
	{
		icmd = 0;
	}

	void operator()(ext::coro coro, const TaskInfo& tinfo)
	{
		taskinfo = tinfo;
		(*this)(coro);
	}

	void operator()(ext::coro coro, const boost::system::error_code& ec)
	{
		if (ec)
			std::cout << "CommandExecuteHelper: Warning: " << ec.message() << "\n";
		(*this)(coro);
	}

	void operator()(ext::coro coro)
	{
		CORO_REENTER(coro) {
			for (icmd = 0; icmd < cmdinfo->list5.size(); ++icmd) {
				clc = cmdinfo->list5[icmd];

				if (clc.empty()) {
					std::cout << "Warning: ignoring empty command\n";
				} else if (clc[0] == "--delete") {
					if (clc.size() == 2)
						AutoUpdater::remove(core->get_io_service(), core->get_work_service(), clc[1]);
					else
						std::cout << "Warning: ignoring --delete with incorrect number of parameters: " << clc.size()-1 << "\n";
				} else if (clc[0] == "--add-extra-build") {
					updateProvider.checkfile(core->get_io_service(), clc[2], clc[1], true);
				} else if (clc[0] == "--add-share" || clc[0] == "--add-shares") {
					for (size_t i = 1; i < clc.size(); ++i) {
						ext::filesystem::path fp(clc[i]);
						if (ext::filesystem::exists(fp))
							core->addLocalShare(fp);
						else
							std::cout << "Warning: tried to share non-existing file/dir: " << fp << "\n";
					}
				} else if (clc[0] == "--download-share") {
					if (clc.size() == 3) {
						share = clc[1];
						CORO_YIELD { // Wait until download started
							sigconn = core->connectSigNewTask(coro(this));
							ShareID hax;
							hax.sid = share;
							for (size_t i = 0; i < core->netmodules.size(); ++i) {
								// try all modules and hackfully only one accepts the share url
								hax.mid = i;
								core->startDownload(hax, clc[2]);
							}
						};
						if (taskinfo.isupload || taskinfo.shareid.sid != share)
							CORO_BREAK; // YIELD above actually forked, get rid of unwanted children here
						sigconn.disconnect(); // got child we wanted, prevent any more

						CORO_YIELD sigconn = core->connectSigTaskStatus(taskinfo.id, coro(this));
						if (taskinfo.status != TASK_STATUS_COMPLETED && taskinfo.status != TASK_STATUS_ERROR)
							CORO_BREAK; // Again get rid of unwanted children
						sigconn.disconnect(); // until we get the one we want

						if (taskinfo.status == TASK_STATUS_COMPLETED) {
							std::cout << "cbTaskProgress: TASK_STATUS_COMPLETED\n";
						} else if (taskinfo.status == TASK_STATUS_ERROR) {
							std::cout << "cbTaskProgress: TASK_STATUS_ERROR\n";
						} else
							throw std::runtime_error("Error!");
					} else
						std::cout << "Warning: ignoring --download-share with incorrect number of parameters: " << clc.size()-1 << "\n";
				} else if (clc[0] == "--notify-socket") {
					if (clc.size() == 2) {
						sock.open(boost::asio::ip::tcp::v4());
						CORO_YIELD sock.async_connect(
							boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), atoi(clc[1].c_str())),
							coro(this)
						);
						sock.close();
					}
				} else if (clc[0] == "--quit") {
					cmdinfo->quit = true;
				}
			}

			{
				boost::function<void(void)> lcb;
				lcb.swap(cb);
				lcb();
			}
		}
	}
};

template <typename T>
struct LocalConnectionBase: public ext::coro::base<T> {
	boost::asio::ip::tcp::socket sock;
	boost::asio::streambuf rbuf;
	std::istream rstream;

	std::string sendstr;
	std::string recvstr;

	struct recvtag {};
	struct sendtag {};

	T* This() { return static_cast<T*>(this); }

	LocalConnectionBase(boost::asio::io_service& service)
		: sock(service), rstream(&rbuf)
	{
	}

	void recvline0(ext::coro coro)
	{
		boost::asio::async_read_until(sock, rbuf, '\x00',
			boost::bind(coro(this), recvtag(), _1, _2)
		);
	}

	void sendline0(ext::coro coro, const std::string& s)
	{
		sendstr = s;
		boost::asio::async_write(sock, boost::asio::buffer(sendstr.c_str(), sendstr.size()+1),
			boost::bind(coro(this), sendtag(), _1, _2)
		);
	}

	void operator()(ext::coro coro, recvtag tag, const boost::system::error_code& ec, size_t transferred)
	{
		if (!ec) {
			recvstr.resize(transferred);
			rstream.read(&recvstr[0], transferred);
			recvstr.resize(transferred-1);
		}
		(*This())(coro, ec);
	}

	void operator()(ext::coro coro, sendtag tag, const boost::system::error_code& ec, size_t transferred)
	{
		(*This())(coro, ec);
	}
};

struct UFTTCore::LocalConnectionHandler: public LocalConnectionBase<LocalConnectionHandler> {
	UFTTCore* core;

	size_t icmd, ncmd;
	size_t iarg, narg;

	CommandLineInfo cmdinfo;

	bool execwait;
	boost::system::error_code sec;

	LocalConnectionHandler(UFTTCore* core_)
		: LocalConnectionBase<LocalConnectionHandler>(core_->get_io_service())
		, core(core_)
	{
		execwait = true;
	}

	void error(const std::string& message)
	{
		std::cout << message << '\n';
	}

	using LocalConnectionBase<LocalConnectionHandler>::operator();
	void operator()(ext::coro coro, const boost::system::error_code& ec = boost::system::error_code())
	{
		if (ec)
			return error(STRFORMAT("LocalConnectionHandler: %s (%d)\n", ec.message(), this->stateval(coro)));
		try {
			CORO_REENTER(coro) {
				CORO_YIELD core->local_listener.async_accept(sock, coro(this));
				(new LocalConnectionHandler(core))->start();

				CORO_YIELD recvline0(coro);
				if (recvstr != "argsL5.0") return error(std::string() + "LocalConnectionHandler: invalid tag: " + recvstr);
				CORO_YIELD recvline0(coro);
				ncmd = boost::lexical_cast<size_t>(recvstr);
				for (icmd = 0; icmd < ncmd; ++icmd) {
					cmdinfo.list5.push_back(CommandLineCommand());
					CORO_YIELD recvline0(coro);
					narg = boost::lexical_cast<size_t>(recvstr);
					for (iarg = 0; iarg < narg; ++iarg) {
						CORO_YIELD recvline0(coro);
						cmdinfo.list5.back().push_back(recvstr);
					}
				}

				// if execwait==true, we wait for command execution, otherwise we fork a child to do it
				if (!execwait) CORO_FORK coro(this)();
				if (execwait || coro.is_child()) {
					CORO_YIELD (new UFTTCore::CommandExecuteHelper(core, &cmdinfo, coro(this)))->start();
					if (!execwait) CORO_BREAK; // !execwait means we were the forked child
				}

				CORO_YIELD sendline0(coro, "0");
				CORO_YIELD sendline0(coro, cmdinfo.quit ? "quit" : "activate");
				if (!cmdinfo.quit)
					CORO_YIELD sendline0(coro, core->mwid);
				else
					CORO_YIELD sendline0(coro, platform::getCurrentPID());
				CORO_YIELD recvline0(coro);
				if (recvstr != "done") return error(std::string() + "LocalConnectionHandler: invalid ack: " + recvstr);
				if (!cmdinfo.quit)
					core->sigGuiCommand(GUI_CMD_SHOW);
				else
					core->sigGuiCommand(GUI_CMD_QUIT);
			}
		} catch (std::exception& e) {
			return error(e.what());
		}
	}
};

struct UFTTCore::LocalConnectionWriter: public LocalConnectionBase<LocalConnectionWriter> {
	boost::asio::io_service& service;
	const boost::asio::ip::tcp::endpoint& local_endpoint;
	const CommandLineInfo& cmdinfo;
	int ret;

	platform::PHANDLE* proc;
	boost::system::error_code sec;

	LocalConnectionWriter(boost::asio::io_service& service_, const boost::asio::ip::tcp::endpoint& local_endpoint_, const CommandLineInfo& cmdinfo_)
	: LocalConnectionBase<LocalConnectionWriter>(service_), service(service_)
	, local_endpoint(local_endpoint_), cmdinfo(cmdinfo_)
	, ret(-1)
	{
	}

	using LocalConnectionBase<LocalConnectionWriter>::operator();
	void operator()(ext::coro coro, const boost::system::error_code& ec = boost::system::error_code())
	{
		if (ec)
			throw std::runtime_error(STRFORMAT("LocalConnectionWriter: %s (%d)\n", ec.message(), this->stateval(coro)));
		CORO_REENTER(coro) {
			CORO_FORK coro(this)();
			if (coro.is_parent()) {
				service.run();
				return;
			}
			sock.open(boost::asio::ip::tcp::v4());
			CORO_YIELD sock.async_connect(local_endpoint, coro(this));
			sendstr.clear();
			sendstr += STRFORMAT("argsL5.0%c%d%c", '\x00', cmdinfo.list5.size(), '\x00');
			BOOST_FOREACH(const CommandLineCommand& cmd, cmdinfo.list5) {
				sendstr += STRFORMAT("%d", cmd.size());
				sendstr += '\x00';
				BOOST_FOREACH(const std::string& str, cmd) {
					sendstr += str;
					sendstr += '\x00';
				}
			}
			sendstr.resize(sendstr.size()-1);
			CORO_YIELD sendline0(coro, sendstr);
			CORO_YIELD recvline0(coro);
			ret =  boost::lexical_cast<int>(recvstr);
			CORO_YIELD recvline0(coro);
			if (recvstr == "quit") {
				CORO_YIELD recvline0(coro);
				proc = platform::getProcessHandle(recvstr);
				CORO_YIELD sendline0(coro, "done");
				ret = platform::waitForProcessExit(proc);
				platform::freeProcessHandle(proc);
			} else if (recvstr == "activate") {
				CORO_YIELD recvline0(coro);
				CORO_YIELD sendline0(coro, "done");
				platform::activateWindow(recvstr);
			} else
				throw std::runtime_error("Unknown mode");
			throw ret;
		}
	}
};

UFTTCore::UFTTCore(UFTTSettingsRef settings_, const CommandLineInfo& cmdinfo)
: settings(settings_)
, local_listener(io_service)
, error_state(0)
{
	boost::asio::ip::tcp::endpoint local_endpoint(boost::asio::ip::address_v4::loopback(), UFTT_PORT-1);
	try {
		local_listener.open(local_endpoint.protocol());
		#ifdef __linux
			// In linux this is needed to be able to listen on a port for which there are still
			// connections in the TIME_WAIT state. Note you still can't actively listen on
			// the same port twice, so detecting running uftt instances still works.
			local_listener.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		#endif
		local_listener.bind(local_endpoint);
		local_listener.listen(16);

		(new LocalConnectionHandler(this))->start();
	} catch (std::exception& pe) {
		try {
			// will throw std::exception on failure, and int with exit code on success
			(new LocalConnectionWriter(io_service, local_endpoint, cmdinfo))->start();
		} catch (std::exception& e) {
			std::cout << "Failed to listen : " << pe.what() << std::endl;
			std::cout << "Failed to connect: " << e.what() << std::endl;
			io_service.reset();
		}
	}
}

void UFTTCore::initialize()
{
	localshares.clear();

	netmodules = NetModuleLinker::getNetModuleList(this);
	for (uint i = 0; i < netmodules.size(); ++i)
		netmodules[i]->setModuleID(i);

	servicerunner = boost::move(boost::thread(boost::bind(&UFTTCore::servicerunfunc, this)));

	ext::filesystem::path apppath = platform::getApplicationPath();
	if (!apppath.empty())
		updateProvider.checkfile(get_io_service(), apppath, get_build_string(), true);

	// Disabled for now because they clobber user state:
	//settings->uftt_send_to.connectChanged(boost::bind(&platform::setSendToUFTTEnabled, _1), true);
	//settings->uftt_desktop_shortcut.connectChanged(boost::bind(&platform::setDesktopShortcutEnabled, _1), true);
	//settings->uftt_quicklaunch_shortcut.connectChanged(boost::bind(&platform::setQuicklaunchShortcutEnabled, _1), true);
	//settings->uftt_startmenu_group.connectChanged(boost::bind(&platform::setStartmenuGroupEnabled, _1), true);
}

void UFTTCore::handleArgsDone(CommandLineInfo* cmdinfo)
{
	if (cmdinfo->quit)
		sigGuiCommand(GUI_CMD_QUIT);
}

void UFTTCore::handleArgs(CommandLineInfo* cmdinfo)
{
	io_service.post(
		(new UFTTCore::CommandExecuteHelper(this, cmdinfo, boost::bind(&UFTTCore::handleArgsDone, this, cmdinfo)))->starter()
	);
}

void UFTTCore::setMainWindowId(const std::string& mwid_)
{
	mwid = mwid_;
}

UFTTCore::~UFTTCore()
{
	io_service.stop();
	servicerunner.join();
}

void UFTTCore::servicerunfunc() {
	boost::asio::io_service::work wobj(io_service);
	io_service.run();
}

SignalConnection UFTTCore::connectSigGuiCommand(const boost::function<void(GuiCommand)>& cb)
{
	return SignalConnection::connect_wait(io_service, sigGuiCommand, cb);
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
	while (p.filename() == ".") p = p.parent_path(); // linux thingy
	std::string name = p.filename();
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
SignalConnection UFTTCore::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	SignalConnection conn;
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		conn += nm->connectSigAddShare(cb);
	return conn;
}

SignalConnection UFTTCore::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	SignalConnection conn;
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		conn += nm->connectSigDelShare(cb);
	return conn;
}

SignalConnection UFTTCore::connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>& cb)
{
	SignalConnection conn;
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		conn += nm->connectSigNewTask(cb);
	return conn;
}

SignalConnection UFTTCore::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	if (tid.mid < netmodules.size())
		return netmodules[tid.mid]->connectSigTaskStatus(tid, cb);
	else
		return SignalConnection();
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

boost::asio::io_service& UFTTCore::get_work_service()
{
	// TODO use seperate service here?
	return boost::asio::use_service<ext::asio::fstream_service<ext::asio::fstream> >(io_service).get_work_service();
}
