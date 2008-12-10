#ifndef UFTT_CORE_H
#define UFTT_CORE_H

#include "Types.h"

#include <string>
#include <map>
#include <vector>

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

struct ShareID {
	std::string sid; // id as string for first step

	bool operator==(const ShareID& o) const
	{
		return (this->sid == o.sid);
	}
};

struct ShareInfo {
	ShareID id;
	std::string name;
	std::string host;
	std::string proto;
	bool isupdate;
	bool islocal;
	ShareInfo() : isupdate(false), islocal(false) {};
};

struct TaskID {
	uint32 mid;
	uint32 cid;
};

struct TaskInfo {
	TaskID id;
	ShareID shareid;
	ShareInfo shareinfo;
	bool isupload;
	std::string status;
	uint64 transferred;
	uint64 size;
	uint32 queue;
};

class UFTTCore {
	private:
		std::map<std::string, boost::filesystem::path> localshares;
		std::vector<boost::shared_ptr<class INetModule> > netmodules;
	public:
		UFTTCore();

		// Local Share management
		void addLocalShare(const std::string& name, const boost::filesystem::path& path);
		void delLocalShare(const ShareID& id);

		void connectSigAddLocalShare(const ShareInfo& info);
		void connectSigDelLocalShare(const ShareID& id);

		boost::filesystem::path getLocalSharePath(const ShareID& id);
		boost::filesystem::path getLocalSharePath(const std::string& id);


		// Remote Share Listing/Downloading
		void connectSigAddShare(const boost::function<void(const ShareInfo&)>&);
		void connectSigDelShare(const boost::function<void(const ShareID&)>&);
		void connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>&);
		void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&);

		void startDownload(const ShareID& sid, const boost::filesystem::path& path);


		// Misc network functions
		void doManualPublish(const std::string& host);
		void doManualQuery(const std::string& host);
		void doRefreshShares();


		// Deprecated but still in use
		void checkForWebUpdates();
		void doSetPeerfinderEnabled(bool enabled);
};

#endif//UFTT_CORE_H
