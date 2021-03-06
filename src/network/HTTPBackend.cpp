#include "HTTPBackend.h"

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(HTTPBackend);

#include <set>

#include "../net-asio/asio_http_request.h"
#include "../util/StrFormat.h"
#include "../util/asio_timer_oneshot.h"

#include "../Globals.h"
#include "../Platform.h"

using namespace std;

class HTTPTask {
	public:
		boost::asio::http_request req;
		ext::filesystem::path path;
		ext::asio::fstream file;
		TaskInfo info;
		boost::signals2::signal<void(const TaskInfo&)> sig_progress;

		HTTPTask(UFTTCore* core)
		: req(core->get_io_service()), file(core->get_io_service())
		{}
};

HTTPBackend::HTTPBackend(UFTTCore* core_)
: core(core_)
, service(core_->get_io_service())
{
	check_update_interval();
}

SignalConnection HTTPBackend::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, sig_new_share, cb);
}

SignalConnection HTTPBackend::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	// TODO: implement this
	return SignalConnection();
}

SignalConnection HTTPBackend::connectSigNewTask(const boost::function<void(const TaskInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, sig_new_task, cb);
}

SignalConnection HTTPBackend::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	return SignalConnection::connect_wait(service, boost::bind(&HTTPBackend::do_connect_sig_task_status, this, tid, cb));
}

void HTTPBackend::doRefreshShares()
{
}

void HTTPBackend::startDownload(const ShareID& sid, const ext::filesystem::path& path)
{
	service.post(boost::bind(&HTTPBackend::do_start_download, this, sid, path));
}

void HTTPBackend::doManualPublish(const std::string& host)
{
}

void HTTPBackend::doManualQuery(const std::string& host)
{
	if (host == "webupdate")
		service.post(boost::bind(&HTTPBackend::check_for_web_updates, this));
}

void HTTPBackend::setModuleID(uint32 mid)
{
	this->mid = mid;
}

void HTTPBackend::notifyAddLocalShare(const LocalShareID& sid)
{
}

void HTTPBackend::notifyDelLocalShare(const LocalShareID& sid)
{
}

// end of interface
// start of internal functions

void HTTPBackend::check_for_web_updates()
{
	//std::string weburl = "http://hackykid.heliohost.org/site/autoupdate.php";
	std::string weburl = "http://uftt.googlecode.com/svn/trunk/site/autoupdate.php";
	//std::string weburl = "http://localhost:8080/site/autoupdate.php";

	HTTPTaskRef task(new HTTPTask(core));
	task->req.async_http_request(weburl, boost::bind(&HTTPBackend::web_update_page_handler, this, _2, task, _1));
}

void HTTPBackend::web_update_page_handler(const boost::system::error_code& err, HTTPTaskRef task, int prog)
{
	if (prog >= 0)
		return;
	if (err) {
		std::cout << "Error checking for web updates: " << err.message() << '\n';
	} else {
		core->getSettingsRef()->lastupdate = boost::posix_time::second_clock::universal_time();

		std::vector<std::pair<std::string, std::string> > builds = AutoUpdater::parseUpdateWebPage(task->req.getContent());
		for (uint i = 0; i < builds.size(); ++i) {
			ShareInfo si;
			si.id.mid = mid;
			si.id.sid = builds[i].second;
			si.host = builds[i].second;
			si.name = platform::makeValidUTF8(builds[i].first);
			si.isupdate = true;
			si.proto = "http";
			si.user = "webupdate";
			shareinfo[si.id] = si;
			sig_new_share(si);
		}
	}
}

void HTTPBackend::check_update_interval()
{
	asio_timer_oneshot(service, boost::posix_time::hours(1), boost::bind(&HTTPBackend::check_update_interval, this));
	//asio_timer_oneshot(service, boost::posix_time::seconds(3), boost::bind(&HTTPBackend::check_update_interval, this));

	UFTTSettingsRef settings = core->getSettingsRef();

	if (settings->webupdateinterval == 0) return;
	int minhours = 30 * 24;
	if (settings->webupdateinterval == 1) minhours = 1 * 24;
	if (settings->webupdateinterval == 2) minhours = 7 * 24;

	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();

	int lasthours = (curtime - settings->lastupdate).hours();

	std::cout << STRFORMAT("last update check: %dh ago (%dh)\n", lasthours, minhours);

	if (lasthours > minhours)
		check_for_web_updates();
}

void HTTPBackend::do_start_download(const ShareID& sid, const ext::filesystem::path& path)
{
	ShareInfo si = shareinfo[sid];
	if (si.id != sid) {
		std::cout << "Invalid Share ID\n";
		return;
	}

	std::string url = sid.sid;

	HTTPTaskRef task(new HTTPTask(core));

	task->info.id.mid = mid;
	task->info.id.cid = tasklist.size();
	task->info.shareid = sid;
	task->info.shareinfo = si;
	task->info.isupload = false;
	task->info.queue = 3;
	task->info.size = 0;
	task->info.status = TASK_STATUS_CONNECTING;
	task->info.transferred = 0;

	task->path = path;

	tasklist.push_back(task);
	sig_new_task(task->info);

	task->req.async_http_request(url, boost::bind(&HTTPBackend::handle_download_progress, this, _2, task, _1));
}

void HTTPBackend::handle_download_progress(const boost::system::error_code& err, HTTPTaskRef task, int prog)
{
	if (prog >= 0) {
		task->info.transferred += prog;
		task->info.size = task->req.getTotalSize();
		task->info.status = TASK_STATUS_TRANSFERRING;
	} else if (err) {
		std::cout << "Failed to download web update: " << err << '\n';
		task->info.error_message = STRFORMAT("Failed: %s", err);
		task->info.status = TASK_STATUS_ERROR;
	} else {
		task->info.size = task->req.getTotalSize();
		task->info.transferred = task->req.getContent().size();
		task->info.queue = 2;
		string fname = task->info.shareinfo.name;
		task->file.async_open(
			task->path / fname,
			ext::asio::fstream::out|ext::asio::fstream::create,
			boost::bind(&HTTPBackend::handle_file_open_done, this, _1, task)
		);
	}
	task->sig_progress(task->info);
}

void HTTPBackend::handle_file_open_done(const boost::system::error_code& err, HTTPTaskRef task)
{
	if (err) {
		std::cout << "Failed to download web update: " << err << '\n';
		task->info.error_message = STRFORMAT("Failed: %s", err);
		task->info.status = TASK_STATUS_ERROR;
	} else {
		task->info.queue = 1;
		boost::asio::async_write(
			task->file,
			boost::asio::buffer(&(task->req.getContent()[0]), task->req.getContent().size()),
			boost::bind(&HTTPBackend::handle_file_write_done, this, _1, task)
		);
	}
	task->sig_progress(task->info);
}

void HTTPBackend::handle_file_write_done(const boost::system::error_code& err,HTTPTaskRef task)
{
	if (err) {
		std::cout << "Failed to download web update: " << err << '\n';
		task->info.error_message = STRFORMAT("Failed: %s", err);
		task->info.status = TASK_STATUS_ERROR;
	} else {
		task->info.queue = 0;
		task->info.status = TASK_STATUS_COMPLETED;
		task->file.close();
	}
	task->sig_progress(task->info);
}

boost::signals2::connection HTTPBackend::do_connect_sig_task_status(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	boost::signals2::connection c;
	if (tid.cid < tasklist.size() && tasklist[tid.cid]) {
		c = tasklist[tid.cid]->sig_progress.connect(cb);
		cb(tasklist[tid.cid]->info);
	}
	return c;
}
