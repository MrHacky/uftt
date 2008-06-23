#include "Types.h"


#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_ipx.h"
#include "net-asio/asio_file_stream.h"
#include "net-asio/ipx_conn.h"
#include "SharedData.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

using namespace std;

using boost::asio::ipx;


#define BUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;

typedef boost::shared_ptr<std::vector<uint8> > shared_vec;

struct filesender {
	std::string name;
	boost::filesystem::path path;

	void init() {
	};

	template <typename Handler>
	bool getbuf(shared_vec buf, const Handler& handler) {
		return false;
	};
};

struct dirsender {
	std::string name;
	boost::filesystem::path path;

	void init() {
	};

	bool getbuf(shared_vec buf, boost::filesystem::path& newpath)
	{
		return false;
	};
};

class SimpleTCPConnection {
	private: 
		friend class SimpleBackend;
		SimpleBackend* backend;

		boost::asio::io_service& service;
		boost::asio::ip::tcp::socket socket;

		std::string sharename;
		boost::filesystem::path sharepath;

		filesender cursendfile;
		std::vector<dirsender> quesenddir;

		void getsharepath(std::string sharename);

	public:
		SimpleTCPConnection(boost::asio::io_service& service_, SimpleBackend* backend_)
			: service(service_)
			, socket(service_)
			, backend(backend_)
		{
		}

		void handle_tcp_accept()
		{
			shared_vec rbuf(new std::vector<uint8>());
			rbuf->resize(4);
			boost::asio::async_read(socket, boost::asio::buffer(&((*rbuf)[0]), rbuf->size()),
				boost::bind(&SimpleTCPConnection::handle_recv_namelen, this, _1, rbuf));
		}

		void sendpath(boost::filesystem::path path, std::string name = "")
		{
			if (name.empty()) name = path.leaf();

			if (!boost::filesystem::exists(path))
				cout << "path does not exist: " << path << '\n';

			if (boost::filesystem::is_directory(path)) {
				dirsender newdir;
				newdir.name = name;
				newdir.path = path;
				newdir.init();
				quesenddir.push_back(newdir);
			} else {
				cursendfile.name = name;
				cursendfile.path = path;
				cursendfile.init();
			}
		}

		void sendfileinfo(const boost::system::error_code& e, shared_vec sbuf, bool filedone) {
			if (e) {
				cout << "error: " << e.message() << '\n';
				return;
			}
			if (filedone) {
				cursendfile.name = "";
			}

			boost::asio::async_write(socket, boost::asio::buffer(&((*sbuf)[0]), sbuf->size()),
				boost::bind(&SimpleTCPConnection::handle_sent_filedirinfo, this, _1, sbuf));
		}

		void checkwhattosend(shared_vec sbuf = shared_vec()) {
			if (!sbuf) sbuf = shared_vec(new std::vector<uint8>());
			if (cursendfile.name != "") {
				cursendfile.getbuf(sbuf, boost::bind(&SimpleTCPConnection::sendfileinfo, this, _1, sbuf, _2));
			} else if (!quesenddir.empty()) {
				boost::filesystem::path newpath;
				
				bool dirdone = quesenddir.back().getbuf(sbuf, newpath);
				if (dirdone) {
					quesenddir.pop_back();
				} else {
					sendpath(newpath);
				}
				boost::asio::async_write(socket, boost::asio::buffer(&((*sbuf)[0]), sbuf->size()),
					boost::bind(&SimpleTCPConnection::handle_sent_filedirinfo, this, _1, sbuf));
			} else {
				cout << "sent it all!\n";
				socket.close();
			}
		}

		void handle_sent_filedirinfo(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				cout << "error: " << e.message() << '\n';
				return;
			}
			checkwhattosend(sbuf);
		};

		void handle_recv_namelen(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				cout << "error: " << e.message() << '\n';
				return;
			}
			uint32 namelen = 0;
			namelen |= (rbuf->at(0) <<  0);
			namelen |= (rbuf->at(1) <<  8);
			namelen |= (rbuf->at(2) << 16);
			namelen |= (rbuf->at(3) << 24);
			rbuf->resize(namelen);
			boost::asio::async_read(socket, boost::asio::buffer(&((*rbuf)[0]), rbuf->size()),
				boost::bind(&SimpleTCPConnection::handle_recv_name, this, _1, rbuf));
		}

		void handle_recv_name(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				cout << "error: " << e.message() << '\n';
				return;
			}
			sharename.clear();
			for (int i = 0; i < rbuf->size(); ++i)
				sharename.push_back(rbuf->at(i));
			cout << "got share name: " << sharename << '\n';
			getsharepath(sharename);
			cout << "got share path: " << sharepath << '\n';
			sendpath(sharepath, sharename);
			checkwhattosend();
		}

		void handle_tcp_connect(std::string name, boost::filesystem::path dlpath)
		{
			sharename = name;
			sharepath = dlpath;

			shared_vec sbuf(new std::vector<uint8>());
/*
			sbuf->push_back(1); // protocol version
			sbuf->push_back(0); // protocol version
			sbuf->push_back(0); // protocol version
			sbuf->push_back(0); // protocol version
*/
			uint32 namelen = name.size();
			sbuf->push_back((namelen >>  0)&0xff);
			sbuf->push_back((namelen >>  8)&0xff);
			sbuf->push_back((namelen >> 16)&0xff);
			sbuf->push_back((namelen >> 24)&0xff);

			for (uint i = 0; i < namelen; ++i)
				sbuf->push_back(name[i]);

			boost::asio::async_write(socket, boost::asio::buffer(&((*sbuf)[0]), sbuf->size()),
				boost::bind(&SimpleTCPConnection::handle_sent_name, this, _1, sbuf));
			//boost::asio::async_write(socket,
			//socket.async_write(

			//cout << "todo: handle tcp connect\n";
		}

		void handle_sent_name(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				cout << "error: " << e.message() << '\n';
				return;
			}
			cout << "sent share name\n";
			sbuf->clear();
		}
};
typedef boost::shared_ptr<SimpleTCPConnection> SimpleTCPConnectionRef;

class SimpleBackend {
	private:
		boost::asio::io_service service;
		boost::asio::ip::udp::socket udpsocket;
		boost::asio::ip::tcp::acceptor tcplistener;
		SimpleTCPConnectionRef newconn;
		std::vector<SimpleTCPConnectionRef> conlist;
		std::map<std::string, boost::filesystem::path> sharelist;
		
		boost::thread servicerunner;

		void servicerunfunc() {
			boost::asio::io_service::work wobj(service);
			service.run();
		}

		void start_tcp_accept()	{
			newconn = SimpleTCPConnectionRef(new SimpleTCPConnection(service, this));
			tcplistener.async_accept(newconn->socket,
				boost::bind(&SimpleBackend::handle_tcp_accept, this, boost::asio::placeholders::error));
		}

		void handle_tcp_accept(const boost::system::error_code& e) {
			if (e) {
				cout << "tcp accept failed: " << e.message() << '\n';
			} else {
				conlist.push_back(newconn);
				newconn->handle_tcp_accept();
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

		void handle_udp_receive(const boost::system::error_code& e, std::size_t len) {
			if (!e) {
				if (len >= 4) {
					uint32 rpver = 
						(udp_recv_buf[0] <<  0) |
						(udp_recv_buf[1] <<  8) |
						(udp_recv_buf[2] << 16) |
						(udp_recv_buf[3] << 24);
					if (rpver == 1 && len >= 5) {
						cout << "got packet type " << (int)udp_recv_buf[4] << "\n";
						switch (udp_recv_buf[4]) {
							case 1: { // type = broadcast;
								typedef std::pair<const std::string, boost::filesystem::path> shareiter;
								BOOST_FOREACH(shareiter& item, sharelist)
								if (item.first.size() < 0xff) {
									uint8 udp_send_buf[1024];
									memcpy(udp_send_buf, udp_recv_buf, 4);
									udp_send_buf[4] = 2;
									udp_send_buf[5] = item.first.size();
									memcpy(&udp_send_buf[6], item.first.data(), item.first.size());
									
									boost::system::error_code err;
									udpsocket.send_to(
										boost::asio::buffer(udp_send_buf, item.first.size()+6), 
										udp_recv_addr,
										0,
										err
									);
									if (err)
										cout << "reply failed: " << err.message() << '\n';
								}
							}; break;
							case 2: { // type = reply;
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl;
										surl += "uftt://";
										surl += udp_recv_addr.address().to_string();
										surl += '/';
										surl += sname;
										sig_new_share(surl);
									}
								}
							}; break;
						}
					}
				}
			} else
				cout << "udp receive failed: " << e.message() << '\n';

			if (!e)
				start_udp_receive();
		}

		void send_broadcast_query() {
			uint8 udp_send_buf[1024];
			uint32 udp_protocol_version = 1;
			udp_send_buf[0] = (udp_protocol_version >>  0) & 0xff;
			udp_send_buf[1] = (udp_protocol_version >>  8) & 0xff;
			udp_send_buf[2] = (udp_protocol_version >> 16) & 0xff;
			udp_send_buf[3] = (udp_protocol_version >> 24) & 0xff;

			udp_send_buf[4] = 1; // type = broadcast;

			boost::system::error_code err;
			udpsocket.send_to(
				boost::asio::buffer(udp_send_buf, 5), 
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("255.255.255.255"), 54345),
				//boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 54345),
				0,
				err
			);

			if (err)
				cout << "broadcast failed: " << err.message() << '\n';
		}

		void add_local_share(std::string name, boost::filesystem::path path)
		{
			if (sharelist[name] != "")
				cout << "warning: replacing share with identical name\n";
			sharelist[name] = path;
			/*
			std::string url = "uftt://127.0.0.1/";
			url += name;
			sig_new_share(url);
			*/
			uint8 udp_send_buf[1024];
			uint32 udp_protocol_version = 1;
			udp_send_buf[0] = (udp_protocol_version >>  0) & 0xff;
			udp_send_buf[1] = (udp_protocol_version >>  8) & 0xff;
			udp_send_buf[2] = (udp_protocol_version >> 16) & 0xff;
			udp_send_buf[3] = (udp_protocol_version >> 24) & 0xff;
			udp_send_buf[4] = 2;
			udp_send_buf[5] = name.size();
			memcpy(&udp_send_buf[6], name.data(), name.size());
			
			boost::system::error_code err;
			udpsocket.send_to(
				boost::asio::buffer(udp_send_buf, name.size()+6), 
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("255.255.255.255"), 54345),
				0,
				err
			);
			if (err)
				cout << "advertise failed: " << err.message() << '\n';

		}

		void download_share(std::string shareurl, boost::filesystem::path dlpath)
		{
			shareurl.erase(0, 7);
			size_t slashpos = shareurl.find_first_of("\\/");
			string host = shareurl.substr(0, slashpos);
			string share = shareurl.substr(slashpos+1);
			SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, this));
			conlist.push_back(newconn);
			boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host), 54345);
			newconn->socket.open(ep.protocol());
			newconn->socket.async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath));
		}

		void dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath)
		{
			if (e) {
				cout << "connect failed: " << e.message() << '\n';
			} else {
				conn->handle_tcp_connect(name, dlpath);
			}
		}
	public:
		SimpleBackend()
			: udpsocket(service)
			, tcplistener(service)
		{
			udpsocket.open(boost::asio::ip::udp::v4());
			udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), 54345));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			tcplistener.open(boost::asio::ip::tcp::v4());
			tcplistener.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), 54345));
			tcplistener.listen();

			start_udp_receive();
			start_tcp_accept();

			boost::thread tt(boost::bind(&SimpleBackend::servicerunfunc, this));
			servicerunner.swap(tt);
		}

		void getsharepath(SimpleTCPConnection* conn, std::string name) {
			conn->sharepath = sharelist[name];
		}

		boost::signal<void(std::string)> sig_new_share;

		void slot_refresh_shares()
		{
			service.post(boost::bind(&SimpleBackend::send_broadcast_query, this));
		}

		void slot_add_local_share(std::string name, boost::filesystem::path path)
		{
			service.post(boost::bind(&SimpleBackend::add_local_share, this, name, path));
		}

		void slot_download_share(std::string shareurl, boost::filesystem::path dlpath)
		{
			service.post(boost::bind(&SimpleBackend::download_share, this, shareurl, dlpath));
		}
};

void SimpleTCPConnection::getsharepath(std::string sharename)
{
	backend->getsharepath(this, sharename);
}

int runtest() {
	try {
		// TEST 0: all dependencies(dlls) have been loaded
		cout << "Loaded dll dependancies...Success\n";

		// todo, put something more here, like:
		cout << "Testing synchronous network I/O...";
		cout << "Skipped\n";

		cout << "Testing asynchronous network I/O...";
		cout << "Skipped\n";

		cout << "Testing asynchronous disk I/O...";
		cout << "Skipped\n";

		cout << "Testing boost libraries...";
		cout << "Skipped\n";

		cout << "Testing qt libraries...";
		cout << "Skipped\n";

		return 0; // everything worked, success!
	} catch (std::exception& e) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: " << e.what() << '\n';
		cout << flush;
		return 1; // error
	} catch (...) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: Unknown\n";
		cout << flush;
		return 1; // error
	}
}

int main( int argc, char **argv )
{
	if (argc > 1 && string(argv[1]) == "--runtest")
		return runtest();

	if (argc > 3 && string(argv[1]) == "--replace")
		cout << "Not implemented yet!\n";

	if (argc > 2 && string(argv[1]) == "--delete")
		cout << "Not implemented yet!\n";

	SimpleBackend backend;
	//NetworkThread thrd1obj;

	QTMain gui(argc, argv);

	gui.BindEvents(&backend);

	//boost::thread thrd1(thrd1obj);

	int ret = gui.run();

	terminating = true;

	//thrd1.join();
	// hax...
	exit(0);

	return ret;
}
