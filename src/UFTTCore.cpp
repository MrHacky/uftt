#include "UFTTCore.h"

#include "network/INetModule.h"
#include "network/NetModuleLinker.h"

#include <boost/foreach.hpp>

typedef boost::shared_ptr<INetModule> INetModuleRef;

UFTTCore::UFTTCore(UFTTSettings& settings_)
: settings(settings_)
, disk_service(io_service)
, error_state(0)
{
	netmodules = NetModuleLinker::getNetModuleList(this);
	for (uint i = 0; i < netmodules.size(); ++i)
		netmodules[i]->setModuleID(i);

	localshares.clear();

	servicerunner = boost::thread(boost::bind(&UFTTCore::servicerunfunc, this)).move();
}

void UFTTCore::servicerunfunc() {
	boost::asio::io_service::work wobj(io_service);
	io_service.run();
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
void UFTTCore::doSetPeerfinderEnabled(bool enabled)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->doSetPeerfinderEnabled(enabled);
}

UFTTSettings& UFTTCore::getSettingsRef()
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
