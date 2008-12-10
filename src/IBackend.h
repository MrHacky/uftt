#ifndef IBACKEND_H
#define IBACKEND_H

#include "Types.h"

#include <string>

#include <boost/function.hpp>
#include <boost/filesystem.hpp>

struct ShareID {
	std::string sid; // id as string for first step

	bool operator==(const ShareID& o) const
	{
		return (this->sid == o.sid);
	}
};

struct TaskID {
	uint32 mid;
	uint32 cid;
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

class IBackend {
	public:
		virtual void connectSigAddShare(const boost::function<void(const ShareInfo&)>&) = 0;
		virtual void connectSigDelShare(const boost::function<void(const ShareID&)>&) = 0;
		virtual void connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>&) = 0;
		virtual void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&) = 0;

		virtual void addLocalShare(const std::string& name, const boost::filesystem::path& path) = 0;
		virtual void doRefreshShares() = 0;
		virtual void startDownload(const ShareID& sid, const boost::filesystem::path& path) = 0;

		virtual void doManualPublish(const std::string& host) = 0;
		virtual void doManualQuery(const std::string& host) = 0;

		virtual void checkForWebUpdates() = 0;

		virtual void doSetPeerfinderEnabled(bool enabled) = 0;
};

#endif//IBACKEND_H
