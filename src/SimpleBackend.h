#ifndef SIMPLE_BACKEND_H
#define SIMPLE_BACKEND_H

#include "Types.h"

#define UFTT_PORT (47189)

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string.hpp>

#include "net-asio/asio_file_stream.h"
#include "net-asio/asio_http_request.h"

#include "SimpleConnection.h"
#include "UFTTSettings.h"

#include "AutoUpdate.h"

#include "../util/StrFormat.h"

#include "IBackend.h"

inline boost::asio::ip::address my_addr_from_string(const std::string& str)
{
	if (str == "255.255.255.255")
		return boost::asio::ip::address_v4::broadcast();
	else
		return boost::asio::ip::address::from_string(str);
}

inline std::string my_datetime_to_string(const boost::posix_time::ptime& td)
{
	return STRFORMAT("%04d-%02d-%02d %02d:%02d:%02d",
		td.date().year(),
		td.date().month(),
		td.date().day(),
		td.time_of_day().hours(),
		td.time_of_day().minutes(),
		td.time_of_day().seconds()
	);
}

class SimpleBackend : public IBackend {
	private:
		boost::asio::io_service service;
		services::diskio_service diskio;
		boost::asio::ip::udp::socket udpsocket;
		boost::asio::ip::tcp::acceptor tcplistener;
		SimpleTCPConnectionRef newconn;
		std::vector<SimpleTCPConnectionRef> conlist;
		std::map<std::string, boost::filesystem::path> sharelist;
		int udpretries;

		boost::thread servicerunner;

		boost::asio::deadline_timer peerfindertimer;
		std::set<boost::asio::ip::address> foundpeers;

		void handle_peerfinder_query(const boost::system::error_code& e, boost::shared_ptr<boost::asio::http_request> request)
		{
			if (e) {
				std::cout << "Failed to get simple global peer list: " << e.message() << '\n';
			} else {
				const std::vector<uint8>& page = request->getContent();

				uint8 content_start[] = "*S*T*A*R*T*\r";
				uint8 content_end[] = "*S*T*O*P*\r";

				typedef std::vector<uint8>::const_iterator vpos;

				vpos spos = std::search(page.begin(), page.end(), content_start   , content_start    + sizeof(content_start   ) - 1);
				vpos epos = std::search(page.begin(), page.end(), content_end     , content_end      + sizeof(content_end     ) - 1);

				if (spos == page.end() || epos == page.end()) {
					std::cout << "Error parsing global peer list.";
					start_peerfinder();
					return;
				}

				spos += sizeof(content_start) - 1;

				std::string content(spos, epos);

				std::vector<std::string> lines;
				boost::split(lines, content, boost::is_any_of("\r\n"));

				std::set<boost::asio::ip::address> addrs;

				settings.foundpeers.clear();
				BOOST_FOREACH(const std::string& line, lines) {
					std::vector<std::string> cols;
					boost::split(cols, line, boost::is_any_of("\t"));
					if (cols.size() >= 2) {
						if (atoi(cols[1].c_str()) == 47189) {
							settings.foundpeers.insert(cols[0]);
						}
					}
				}
			}

			start_peerfinder();
		}

		void handle_peerfinder_timer(const boost::system::error_code& e)
		{
			if (e) {
				std::cout << "handle_peerfinder_timer: " << e.message() << '\n';
			} else {
				if (settings.enablepeerfinder) {
					settings.prevpeerquery = settings.lastpeerquery;
					settings.lastpeerquery = boost::posix_time::second_clock::universal_time();

					std::string url;
					//url = "http://hackykid.heliohost.org/site/bootstrap.php";
					url = "http://hackykid.awardspace.com/site/bootstrap.php";
					url += "?reg=1&type=simple&class=1wdvhi09ehvnazmq23jd";

					boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, url));
					request->setHandler(boost::bind(&SimpleBackend::handle_peerfinder_query, this, boost::asio::placeholders::error, request));
				}
			}
		}

		void start_peerfinder(bool enabled)
		{
			if (settings.enablepeerfinder != enabled) {
				settings.enablepeerfinder = enabled;
				start_peerfinder();
			}
		}

		void start_peerfinder()
		{
			if (!settings.enablepeerfinder) return;

			boost::posix_time::ptime dl;
			if (settings.lastpeerquery - settings.prevpeerquery > boost::posix_time::minutes(55))
				dl = settings.lastpeerquery + boost::posix_time::seconds(20);
			else
				dl = settings.lastpeerquery + boost::posix_time::minutes(50);

			peerfindertimer.expires_at(dl);
			peerfindertimer.async_wait(boost::bind(&SimpleBackend::handle_peerfinder_timer, this, _1));

			std::cout << "Peer check at: " << my_datetime_to_string(dl) << '\n';

			uint8 udp_send_buf[5];
			udp_send_buf[0] = (1 >>  0) & 0xff;
			udp_send_buf[1] = (1 >>  8) & 0xff;
			udp_send_buf[2] = (1 >> 16) & 0xff;
			udp_send_buf[3] = (1 >> 24) & 0xff;
			udp_send_buf[4] = 4;

			boost::system::error_code err;
			BOOST_FOREACH(const std::string peer, settings.foundpeers) {
				try {
					const boost::asio::ip::udp::endpoint ep(my_addr_from_string(peer), UFTT_PORT);
					std::cout << "Sending (4) to: " << ep << '\n';
					udpsocket.send_to(
						boost::asio::buffer(udp_send_buf),
						ep,
						0
						,err
					);
				} catch (...) {
				}
			}
		}

		void servicerunfunc() {
			boost::asio::io_service::work wobj(service);
			service.run();
		}

		void start_tcp_accept() {
			newconn = SimpleTCPConnectionRef(new SimpleTCPConnection(service, this));
			tcplistener.async_accept(newconn->socket,
				boost::bind(&SimpleBackend::handle_tcp_accept, this, boost::asio::placeholders::error));
		}

		void handle_tcp_accept(const boost::system::error_code& e) {
			if (e) {
				std::cout << "tcp accept failed: " << e.message() << '\n';
			} else {
				std::cout << "handling tcp accept\n";
				conlist.push_back(newconn);
				newconn->handle_tcp_accept();
				TaskInfo tinfo;
				tinfo.shareinfo.name = "Upload";
				tinfo.id.id = conlist.size()-1;
				tinfo.isupload = true;
				sig_new_task(tinfo);
				start_tcp_accept();
			}
		}

		uint8 udp_recv_buf[1024];
		boost::asio::ip::udp::endpoint udp_recv_addr;
		void start_udp_receive() {
			udpsocket.async_receive_from(boost::asio::buffer(udp_recv_buf), udp_recv_addr,
				boost::bind(&SimpleBackend::handle_udp_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
			);
		}

		std::set<uint32> parseVersions(uint8* base, uint start, uint len)
		{
			std::set<uint32> versions;
			uint32 i = start;
			if (i < len) {
				uint max = base[i++];
				for (uint num = 0; num < max && i+8 <= len; ++num, i += 8) {
					uint32 lver = (base[i+0] <<  0) | (base[i+1] <<  8) | (base[i+2] << 16) | (base[i+3] << 24);
					uint32 hver = (base[i+4] <<  0) | (base[i+5] <<  8) | (base[i+6] << 16) | (base[i+7] << 24);
					if (lver <= hver && (hver-lver) < 100)
						for (uint32 j = lver; j <= hver; ++j)
							versions.insert(j);
				}
			}
			if (versions.empty()) versions.insert(1);
			return versions;
		}

		void handle_udp_receive(const boost::system::error_code& e, std::size_t len) {
			if (!e) {
				udpretries = 10;
				if (len >= 4) {
					uint32 rpver = (udp_recv_buf[0] <<  0) | (udp_recv_buf[1] <<  8) | (udp_recv_buf[2] << 16) | (udp_recv_buf[3] << 24);
					if (len > 4) {
						std::cout << "got packet type " << (int)udp_recv_buf[4] << " from " << udp_recv_addr << "\n";
						switch (udp_recv_buf[4]) {
							case 1: if (rpver == 1) { // type = broadcast;
								uint32 vstart = 5;
								if (len > 5)
									vstart = 6 + udp_recv_buf[5];

								std::set<uint32> versions = parseVersions(udp_recv_buf, vstart, len);

								       if (versions.count(4)) {
									send_publishes(udp_recv_addr, 2, true);
								} else if (versions.count(3)) {
									send_publishes(udp_recv_addr, 0, true);
								} else if (versions.count(2)) {
									send_publishes(udp_recv_addr, 1, true);
								} else if (versions.count(1)) {
									send_publishes(udp_recv_addr, 1, len >= 6 && udp_recv_buf[5] > 0 && len-6 >= udp_recv_buf[5]);
								}
							}; break;
							case 2: { // type = reply;
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::set<uint32> versions;
										if (rpver == 1)
											versions = parseVersions(udp_recv_buf, slen+6, len);
										versions.insert(rpver);
										uint32 sver = 0;
										if (versions.count(2))
											sver = 2;
										else if (versions.count(1))
											sver = 1;

										if (sver > 0) {
											std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
											std::string surl = STRFORMAT("uftt-v%d://%s/%s", sver, udp_recv_addr.address().to_string(), sname);
											ShareInfo sinfo;
											sinfo.name = sname;
											sinfo.id.sid = surl;
											sinfo.islocal = false;
											sig_new_share(sinfo);
										}
									}
								}
							}; break;
							case 3: { // type = autoupdate share
								if (len >= 6 && (rpver == 1)) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl = STRFORMAT("uftt-v%d://%s/%s", 1, udp_recv_addr.address().to_string(), sname);;
										sig_new_autoupdate(surl);
									}
								}
							}; break;
							case 4: { // peerfinder query
								boost::system::error_code err;
								uint8 udp_send_buf[5];
								udp_send_buf[0] = (1 >>  0) & 0xff;
								udp_send_buf[1] = (1 >>  8) & 0xff;
								udp_send_buf[2] = (1 >> 16) & 0xff;
								udp_send_buf[3] = (1 >> 24) & 0xff;
								udp_send_buf[4] = 5;
								udpsocket.send_to(
									boost::asio::buffer(udp_send_buf),
									udp_recv_addr,
									0
									,err
								);
							};// intentional fallthrough break;
							case 5: { // peerfinder reply
								foundpeers.insert(udp_recv_addr.address());
								settings.foundpeers.insert(STRFORMAT("%s", udp_recv_addr.address()));
								send_query(udp_recv_addr);
							}; break;
						}
					}
				}
			} else {
				if (udpretries == 10) {
					// ignore first message
				} else
					std::cout << "udp receive failed: " << e.message() << '\n';
				--udpretries;
			}

			if (!e || udpretries > 0)
				start_udp_receive();
			else
				std::cout << "retry limit reached, giving up on receiving udp packets\n";
		}

		void send_query(boost::asio::ip::udp::endpoint ep) {
			uint8 udp_send_buf[1024];
			udp_send_buf[ 0] = 0x01;
			udp_send_buf[ 1] = 0x00;
			udp_send_buf[ 2] = 0x00;
			udp_send_buf[ 3] = 0x00;
			udp_send_buf[ 4] = 0x01;
			udp_send_buf[ 5] = 0x01;
			udp_send_buf[ 6] = 0x00;
			udp_send_buf[ 7] = 0x00;

			std::set<uint32> qversions;
			qversions.insert(1);
			qversions.insert(2);
			qversions.insert(4);

			int plen = 8;
			BOOST_FOREACH(uint32 ver, qversions) {
				udp_send_buf[plen+0] = udp_send_buf[plen+4] = (ver >>  0) & 0xff;
				udp_send_buf[plen+1] = udp_send_buf[plen+5] = (ver >>  8) & 0xff;
				udp_send_buf[plen+2] = udp_send_buf[plen+6] = (ver >> 16) & 0xff;
				udp_send_buf[plen+3] = udp_send_buf[plen+7] = (ver >> 24) & 0xff;
				++udp_send_buf[7];
				plen += 8;
			}

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, plen),
					ep,
					0
					,err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, plen), ep.port(), 0, err);

			if (err)
				std::cout << "query failed: " << err.message() << '\n';
		}

		void send_publish(const boost::asio::ip::udp::endpoint& ep, const std::string& name, int sharever, bool isbuild = false)
		{
			uint8 udp_send_buf[1024];
			udp_send_buf[0] = (sharever >>  0) & 0xff;
			udp_send_buf[1] = (sharever >>  8) & 0xff;
			udp_send_buf[2] = (sharever >> 16) & 0xff;
			udp_send_buf[3] = (sharever >> 24) & 0xff;
			udp_send_buf[4] = isbuild ? 3 : 2;
			udp_send_buf[5] = name.size();
			memcpy(&udp_send_buf[6], name.data(), name.size());

			uint32 plen = name.size()+6;
			if (!isbuild && sharever == 1 && ep.address() == boost::asio::ip::address_v4::broadcast()) {
				std::set<uint32> sversions;
				sversions.insert(1);
				sversions.insert(2);

				udp_send_buf[plen++] = sversions.size();

				BOOST_FOREACH(uint32 ver, sversions) {
					udp_send_buf[plen+0] = udp_send_buf[plen+4] = (ver >>  0) & 0xff;
					udp_send_buf[plen+1] = udp_send_buf[plen+5] = (ver >>  8) & 0xff;
					udp_send_buf[plen+2] = udp_send_buf[plen+6] = (ver >> 16) & 0xff;
					udp_send_buf[plen+3] = udp_send_buf[plen+7] = (ver >> 24) & 0xff;
					//++udp_send_buf[7];
					plen += 8;
				}
			}

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, plen),
					ep,
					0,
					err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, plen), ep.port(), 0, err);
			if (err)
				std::cout << "publish of '" << name << "' to '" << ep << "'failed: " << err.message() << '\n';
		}

		void send_publishes(const boost::asio::ip::udp::endpoint& ep, int sharever, bool sendbuilds) {
			typedef std::pair<const std::string, boost::filesystem::path> shareiter;
			if (sharever > 0)
				BOOST_FOREACH(shareiter& item, sharelist)
					if (item.first.size() < 0xff && !item.second.empty())
						send_publish(ep, item.first, sharever);

			if (sendbuilds)
				BOOST_FOREACH(const std::string& name, updateProvider.getAvailableBuilds())
					send_publish(ep, name, 1, true);
		}

		void add_local_share(std::string name, boost::filesystem::path path)
		{
			if (sharelist[name] != "")
				std::cout << "warning: replacing share with identical name\n";
			sharelist[name] = path;

			send_publish(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), name, 1);
			ShareInfo sinfo;
			sinfo.id.sid = path.string();
			sinfo.name = name;
			sinfo.islocal = true;
			sig_new_share(sinfo);
		}

		void download_share(const ShareID& sid, const boost::filesystem::path& dlpath);

		void dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath, uint32 version)
		{
			if (e) {
				std::cout << "connect failed: " << e.message() << '\n';
			} else {
				std::cout << "Connected!\n";
				conn->handle_tcp_connect(name, dlpath, version);
			}
		}

		std::vector<boost::asio::ip::address> get_broadcast_adresses();

		template<typename BUF>
		void send_udp_broadcast(boost::asio::ip::udp::socket& sock, BUF buf, uint16 port, int flags, boost::system::error_code& err)
		{
			std::vector<boost::asio::ip::address> adresses;
			adresses = this->get_broadcast_adresses();
			BOOST_FOREACH(boost::asio::ip::address& addr, adresses) {
				udpsocket.send_to(
					buf,
					boost::asio::ip::udp::endpoint(addr, port),
					flags,
					err
				);
				if (err)
					std::cout << "broadcast to (" << addr << ") failed: " << err.message() << '\n';
			}
		}
		void attach_progress_handler(const TaskID& tid, const boost::function<void(const TaskInfo&)>& cb)
		{
			// TODO: fix this
			int num = (int)tid.id;
			conlist[num]->sig_progress.connect(cb);
		}

		void check_for_web_updates()
		{
			//std::string weburl = "http://hackykid.heliohost.org/site/autoupdate.php";
			std::string weburl = "http://uftt.googlecode.com/svn/trunk/site/autoupdate.php";
			//std::string weburl = "http://localhost:8080/site/autoupdate.php";

			boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, weburl));
			request->setHandler(boost::bind(&SimpleBackend::web_update_page_handler, this, boost::asio::placeholders::error, request));
		}

		void web_update_page_handler(const boost::system::error_code& err, boost::shared_ptr<boost::asio::http_request> req)
		{
			if (err) {
				std::cout << "Error checking for web updates: " << err.message() << '\n';
			} else
			{
				std::vector<std::pair<std::string, std::string> > builds = AutoUpdater::parseUpdateWebPage(req->getContent());
				for (uint i = 0; i < builds.size(); ++i) {
					// TODO: create shares & notify people...
					//handler(builds[i].first, builds[i].second);
				}
			}
		}

		void download_web_update(std::string url, boost::function<void(const boost::system::error_code&, boost::shared_ptr<boost::asio::http_request>) > handler)
		{
			boost::shared_ptr<boost::asio::http_request> request(new boost::asio::http_request(service, url));
			request->setHandler(boost::bind(handler, boost::asio::placeholders::error, request));
		}

	public:
		UFTTSettings& settings; // TODO: remove need for this hack!

		SimpleBackend(UFTTSettings& settings_)
			: diskio(service)
			, udpsocket(service)
			, tcplistener(service)
			, settings(settings_)
			, peerfindertimer(service)
			, udpretries(10)
		{
			gdiskio = &diskio;
			udpsocket.open(boost::asio::ip::udp::v4());
			udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			tcplistener.open(boost::asio::ip::tcp::v4());
			tcplistener.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			tcplistener.listen(16);

			// bind autoupdater
			updateProvider.newbuild.connect(
				boost::bind(&SimpleBackend::send_publish, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), _1, 1, true)
			);

			start_udp_receive();
			start_tcp_accept();

			start_peerfinder();

			boost::thread tt(boost::bind(&SimpleBackend::servicerunfunc, this));
			servicerunner.swap(tt);
		}

		boost::filesystem::path getsharepath(std::string name) {
			return sharelist[name]; // TODO: remove need for this hack!
		}

	private:
		// old interface now..
		boost::signal<void(const ShareInfo&)> sig_new_share;
		boost::signal<void(const TaskInfo&)> sig_new_task;
		boost::signal<void(std::string)> sig_new_autoupdate; // TODO: remove this signal?

		void slot_add_local_share(std::string name, boost::filesystem::path path)
		{
			service.post(boost::bind(&SimpleBackend::add_local_share, this, name, path));
		}

	// new interface starts here!
	public:

		virtual void connectSigAddShare(const boost::function<void(const ShareInfo&)>&);
		virtual void connectSigDelShare(const boost::function<void(const ShareID&)>&);
		virtual void connectSigNewTask(const boost::function<void(const TaskInfo&)>&);
		virtual void connectSigTaskStatus(const TaskID& tid, const boost::function<void(const TaskInfo&)>&);

		virtual void addLocalShare(const std::string& name, const boost::filesystem::path& path);
		virtual void doRefreshShares();
		virtual void startDownload(const ShareID& sid, const boost::filesystem::path& path);

		virtual void doManualPublish(const std::string& host);
		virtual void doManualQuery(const std::string& host);

		virtual void checkForWebUpdates();

		virtual void doSetPeerfinderEnabled(bool enabled);
};

#endif//SIMPLE_BACKEND_H
