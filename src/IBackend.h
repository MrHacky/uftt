#ifndef IBACKEND_H
#define IBACKEND_H

#include "Types.h"

#include <string>

#include <boost/function.hpp>
#include <boost/filesystem.hpp>

#include "UFTTCore.h"

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
