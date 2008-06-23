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

class SimpleTCPConnection {
	private: 
		friend class SimpleBackend;

		boost::asio::io_service& service;
		boost::asio::ip::tcp::socket socket;

	public:
		SimpleTCPConnection(boost::asio::io_service& service_)
			: service(service_)
			, socket(service_)
		{
		}

		void handle_tcp_accept()
		{
			cout << "todo: handle tcp accept\n";
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
			newconn = SimpleTCPConnectionRef(new SimpleTCPConnection(service));
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

		boost::signal<void(std::string)> sig_new_share;

		void slot_refresh_shares()
		{
			service.post(boost::bind(&SimpleBackend::send_broadcast_query, this));
		}

		void slot_add_local_share(std::string name, boost::filesystem::path path)
		{
			service.post(boost::bind(&SimpleBackend::add_local_share, this, name, path));
		}

		void slot_download_share()
		{
		}
};

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

	if (argc > 2 && string(argv[1]) == "--replace")
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
