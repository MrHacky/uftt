#include "UFTTCore.h"

#include "network/INetModule.h"
#include "network/NetModuleLinker.h"

#include <boost/foreach.hpp>

typedef boost::shared_ptr<INetModule> INetModuleRef;

UFTTCore::UFTTCore(UFTTSettings& settings_)
: settings(settings_)
{
	netmodules = NetModuleLinker::getNetModuleList(this);
	for (uint i = 0; i < netmodules.size(); ++i)
		netmodules[i]->setModuleID(i);
}

// Local Share management
void UFTTCore::addLocalShare(const std::string& name, const boost::filesystem::path& path)
{
	// TODO: locking
	localshares[name] = path;
}

void UFTTCore::delLocalShare(const ShareID& id)
{
	// TODO: locking
	localshares[id.sid] = "";
}

void UFTTCore::connectSigAddLocalShare(const ShareInfo& info)
{
	// TODO: implement this
}

void UFTTCore::connectSigDelLocalShare(const ShareID& id)
{
	// TODO: implement this
}

boost::filesystem::path UFTTCore::getLocalSharePath(const ShareID& id)
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
void UFTTCore::checkForWebUpdates()
{
}

void UFTTCore::doSetPeerfinderEnabled(bool enabled)
{
	BOOST_FOREACH(INetModuleRef nm, netmodules)
		nm->doSetPeerfinderEnabled(enabled);
}

UFTTSettings& UFTTCore::getSettingsRef()
{
	return settings;
}
