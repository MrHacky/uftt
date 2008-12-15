#ifndef UFTT_CORE_H
#define UFTT_CORE_H

#include "Types.h"

#include <string>
#include <map>
#include <vector>

#include <boost/asio.hpp>
#include "../net-asio/asio_file_stream.h"

#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include "UFTTSettings.h"

struct LocalShareID {
	std::string sid;
};

struct LocalShareInfo {
	LocalShareID id;
	boost::filesystem::path path;

};

struct ShareID {
	uint32 mid;
	std::string sid; // id as string for first step

	bool operator==(const ShareID& o) const
	{
		return (this->sid == o.sid) && (this->mid == o.mid);
	}
};

struct ShareInfo {
	ShareID id;
	std::string name;
	std::string host;
	std::string proto;
	std::string user;
	bool isupdate;
	ShareInfo() : isupdate(false) {};
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
		UFTTSettings& settings;

		boost::asio::io_service io_service;
		services::diskio_service disk_service;
		boost::thread servicerunner;

		void servicerunfunc();
	public:
		UFTTCore(UFTTSettings& settings_);

		// Local Share management
		void addLocalShare(const std::string& name, const boost::filesystem::path& path);
		void delLocalShare(const LocalShareID& id);

		void connectSigAddLocalShare(const LocalShareInfo& info);
		void connectSigDelLocalShare(const LocalShareID& id);

		boost::filesystem::path getLocalSharePath(const LocalShareID& id);
		boost::filesystem::path getLocalSharePath(const std::string& id);

		std::vector<std::string> getLocalShares();

		// Remote Share Listing/Downloading
		void connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb);
		void connectSigDelShare(const boost::function<void(const ShareID&)>& cb);
		void connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>& cb);
		void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb);

		void startDownload(const ShareID& sid, const boost::filesystem::path& path);


		// Misc network functions
		void doManualPublish(const std::string& host);
		void doManualQuery(const std::string& host);
		void doRefreshShares();


		// Deprecated but still in use
		void checkForWebUpdates();
		void doSetPeerfinderEnabled(bool enabled);
		UFTTSettings& getSettingsRef();

		// Service getters
		boost::asio::io_service& get_io_service();
		services::diskio_service& get_disk_service();
};

#endif//UFTT_CORE_H
