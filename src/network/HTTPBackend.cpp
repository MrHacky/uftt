#include "HTTPBackend.h"

#include "NetModuleLinker.h"
REGISTER_NETMODULE_CLASS(HTTPBackend);

#include <set>

#include "../net-asio/asio_http_request.h"
#include "../util/StrFormat.h"
#include "../util/asio_timer_oneshot.h"

#include "../Globals.h"

using namespace std;

class HTTPTask {
	public:
		boost::asio::http_request req;
		boost::filesystem::path path;
		services::diskio_filetype file;
		TaskInfo info;
		boost::signal<void(const TaskInfo&)> sig_progress;

		HTTPTask(UFTTCore* core)
		: req(core->get_io_service()), file(core->get_disk_service())
		{}
};

HTTPBackend::HTTPBackend(UFTTCore* core_)
: core(core_)
, service(core_->get_io_service())
{
	check_update_interval();
}

void HTTPBackend::connectSigAddShare(const boost::function<void(const ShareInfo&)>& cb)
{
	sig_new_share.connect(cb);
}

void HTTPBackend::connectSigDelShare(const boost::function<void(const ShareID&)>& cb)
{
	// TODO: implement this
}

void HTTPBackend::connectSigNewTask(const boost::function<void(const TaskInfo&)>& cb)
{
	sig_new_task.connect(cb);
}

void HTTPBackend::connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	service.post(boost::bind(&HTTPBackend::do_connect_sig_task_status, this, tid, cb));
}

void HTTPBackend::doRefreshShares()
{
}

void HTTPBackend::startDownload(const ShareID& sid, const boost::filesystem::path& path)
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

void HTTPBackend::doSetPeerfinderEnabled(bool enabled)
{
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
		core->getSettingsRef().lastupdate = boost::posix_time::second_clock::universal_time();

		std::vector<std::pair<std::string, std::string> > builds = AutoUpdater::parseUpdateWebPage(task->req.getContent());
		for (uint i = 0; i < builds.size(); ++i) {
			ShareInfo si;
			si.id.mid = mid;
			si.id.sid = builds[i].second;
			si.host = builds[i].second;
			si.name = builds[i].first;
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

	UFTTSettings& settings = core->getSettingsRef();

	if (settings.webupdateinterval == 0) return;
	int minhours = 30 * 24;
	if (settings.webupdateinterval == 1) minhours = 1 * 24;
	if (settings.webupdateinterval == 2) minhours = 7 * 24;

	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();

	int lasthours = (curtime - settings.lastupdate).hours();

	std::cout << STRFORMAT("last update check: %dh ago (%dh)\n", lasthours, minhours);

	if (lasthours > minhours)
		check_for_web_updates();
}

void HTTPBackend::do_start_download(const ShareID& sid, const boost::filesystem::path& path)
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
	task->info.status = "Downloading";
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
		task->info.size = task->req.totalsize;
	} else if (err) {
		std::cout << "Failed to download web update: " << err << '\n';
		task->info.status = STRFORMAT("Failed: %s", err);
	} else {
		task->info.size = task->req.totalsize;
		task->info.transferred = task->req.getContent().size();
		task->info.queue = 2;
		string fname = task->info.shareinfo.name;
		if (fname.find("win32") != string::npos) fname += ".exe";
		if (fname.find("-deb-") != string::npos) fname += ".deb.signed";
		core->get_disk_service().async_open_file(
			task->path / fname,
			services::diskio_filetype::out|services::diskio_filetype::create,
			task->file,
			boost::bind(&HTTPBackend::handle_file_open_done, this, _1, task)
		);
	}
	task->sig_progress(task->info);
}

void HTTPBackend::handle_file_open_done(const boost::system::error_code& err, HTTPTaskRef task)
{
	if (err) {
		std::cout << "Failed to download web update: " << err << '\n';
		task->info.status = STRFORMAT("Failed: %s", err);
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
		task->info.status = STRFORMAT("Failed: %s", err);
	} else {
		task->info.queue = 0;
		task->info.status = "Completed";
		task->file.close();
	}
	task->sig_progress(task->info);
}

void HTTPBackend::do_connect_sig_task_status(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
{
	if (tid.cid < tasklist.size() && tasklist[tid.cid]) {
		tasklist[tid.cid]->sig_progress.connect(cb);
		tasklist[tid.cid]->sig_progress(tasklist[tid.cid]->info);
	}
}
