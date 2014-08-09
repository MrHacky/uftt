#ifndef HTTP_BACKEND_H
#define HTTP_BACKEND_H

#include "../Types.h"

#include <vector>

#include <boost/signals2/signal.hpp>
#include <boost/shared_ptr.hpp>

#include "INetModule.h"
#include "../UFTTCore.h"

class HTTPTask;
typedef boost::shared_ptr<HTTPTask> HTTPTaskRef;

class HTTPBackend: public INetModule {
	private:
		UFTTCore* core;
		uint32 mid;
		boost::asio::io_service& service;

		boost::signals2::signal<void(const ShareInfo&)> sig_new_share;
		boost::signals2::signal<void(const TaskInfo&)> sig_new_task;

		std::vector<HTTPTaskRef> tasklist;

		std::map<ShareID, ShareInfo> shareinfo;

		void check_update_interval();
		void check_for_web_updates();
		void web_update_page_handler(const boost::system::error_code& err, HTTPTaskRef task, int prog);

		void do_start_download(const ShareID& sid, const ext::filesystem::path& path);
		void handle_download_progress(const boost::system::error_code& err, HTTPTaskRef task, int prog);
		void handle_file_open_done(const boost::system::error_code& err, HTTPTaskRef task);
		void handle_file_write_done(const boost::system::error_code& err, HTTPTaskRef task);

		boost::signals2::connection do_connect_sig_task_status(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb);
	public:
		HTTPBackend(UFTTCore* core_);
	public:
		// interface starts here!

		virtual SignalConnection connectSigAddShare(const boost::function<void(const ShareInfo&)>&);
		virtual SignalConnection connectSigDelShare(const boost::function<void(const ShareID&)>&);
		virtual SignalConnection connectSigNewTask(const boost::function<void(const TaskInfo&)>&);
		virtual SignalConnection connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&);

		virtual void doRefreshShares();
		virtual void startDownload(const ShareID& sid, const ext::filesystem::path& path);

		virtual void doManualPublish(const std::string& host);
		virtual void doManualQuery(const std::string& host);

		virtual void notifyAddLocalShare(const LocalShareID& sid);
		virtual void notifyDelLocalShare(const LocalShareID& sid);

		virtual void setModuleID(uint32 mid);
};

#endif//HTTP_BACKEND_H
