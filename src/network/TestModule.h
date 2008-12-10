#ifndef TESTMODULE_H
#define TESTMODULE_H

#include "INetModule.h"

class TestModule: public INetModule {
	public:
		TestModule(UFTTCore* core);

		void connectSigAddShare(const boost::function<void(const ShareInfo&)>&) {};
		void connectSigDelShare(const boost::function<void(const ShareID&)>&) {};
		void connectSigNewTask(const boost::function<void(const TaskInfo& tinfo)>&) {};
		void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&) {};

		void startDownload(const ShareID& sid, const boost::filesystem::path& path) {};

		void doRefreshShares() {};
		void doManualPublish(const std::string& host) {};
		void doManualQuery(const std::string& host) {};

		void doSetPeerfinderEnabled(bool enabled) {};

		void setModuleID(uint32 mid) {};
};

#endif//TESTMODULE_H
