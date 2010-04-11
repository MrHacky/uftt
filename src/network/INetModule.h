#ifndef INETMODULE_H
#define INETMODULE_H

#include "../UFTTCore.h"

class INetModule {
	public:
		virtual void connectSigAddShare(const boost::function<void(const ShareInfo&)>&) = 0;
		virtual void connectSigDelShare(const boost::function<void(const ShareID&)>&) = 0;
		virtual void connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>&) = 0;
		virtual void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&) = 0;

		virtual void startDownload(const ShareID& sid, const ext::filesystem::path& path) = 0;

		virtual void doRefreshShares() = 0;
		virtual void doManualPublish(const std::string& host) = 0;
		virtual void doManualQuery(const std::string& host) = 0;

		virtual void notifyAddLocalShare(const LocalShareID& sid) = 0;
		virtual void notifyDelLocalShare(const LocalShareID& sid) = 0;

		virtual void setModuleID(uint32 mid) = 0;
};

#endif//INETMODULE_H
