#include "Types.h"

#include "util/UPXFix.h"

#include "UFTTGui.h"
#include "BuildString.h"
#include "AutoUpdate.h"
#include "UFTTSettings.h"
#include "UFTTSettingsLegacy.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#include <boost/random/linear_congruential.hpp>

#include "UFTTCore.h"

#include <fstream>

#include <string>

#include "Platform.h"

using namespace std;

// evil global variables
AutoUpdater updateProvider;
boost::rand48 rng;

#define BUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;

			struct settrue {
				bool* value;
				settrue(bool* value_) : value(value_) {};
				void operator()(const boost::system::error_code& ec, const size_t len = 0)
				{
					boost::asio::detail::throw_error(ec);
					*value = true;
				}
				typedef void result_type;
			};

int runtest() {
	try {
		// TEST 0: all dependencies(dlls) have been loaded
		cout << "Loaded dll dependancies...Success\n";

		// todo, put something more here, like:
		cout << "Testing (a)synchronous network I/O...";
		{
			char sstr[] = "sendstring";
			char rstr[] = "1234567890";
			bool recvstarted = false;
			bool received = false;
			bool connected = false;
			bool accepted = false;
			bool sent = false;
			bool timedout = false;



			boost::asio::io_service service;
			boost::asio::ip::tcp::acceptor acceptor(service);
			boost::asio::ip::tcp::socket ssock(service);
			boost::asio::ip::tcp::socket rsock(service);

			acceptor.open(boost::asio::ip::tcp::v4());
			acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 23432));
			acceptor.listen(16);

			acceptor.async_accept(ssock, settrue(&accepted));

			rsock.open(boost::asio::ip::tcp::v4());
			rsock.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 23432), settrue(&connected));

			boost::asio::deadline_timer wd(service);
			wd.expires_from_now(boost::posix_time::seconds(10));
			wd.async_wait(settrue(&timedout));

			do {
				service.run_one();
				if (accepted && !sent) {
					// sync write here
					boost::asio::write(ssock, boost::asio::buffer(sstr));
					sent = true;
				}
				if (connected && !recvstarted) {
					// async read here
					boost::asio::async_read(rsock, boost::asio::buffer(rstr), settrue(&received));
					recvstarted = true;
				}
				//if (tries == 50 && !connected) {
				//	rsock.async_connect(acceptor.local_endpoint(), settrue(&connected));
				//}
			} while (!timedout && !received);
			//cout << "t: " << tries << '\n';
			//cout << "s: " << sstr << '\n';
			//cout << "r: " << rstr << '\n';
			if (timedout)
				cout << "Timed out...";
			if (!connected)
				throw std::runtime_error("connect failed");
			if (!accepted)
				throw std::runtime_error("accept failed");
			if (!sent)
				throw std::runtime_error("send failed");
			if (!recvstarted || !received)
				throw std::runtime_error("receive failed");
			if (string(sstr) != string(rstr))
				throw std::runtime_error("transfer failed");
		}
		cout << "Success\n";

		cout << "Testing udp broadcast...";
		{
			char sstr[] = "sendstring";
			//char rstr[] = "1234567890";
			boost::asio::io_service service;
			boost::asio::ip::udp::socket udpsocket(service);

			udpsocket.open(boost::asio::ip::udp::v4());
			//udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), 23432));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			udpsocket.send_to(
				boost::asio::buffer(sstr),
				boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), 23432)
				//boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 23432),
				,0
			);


		}
		cout << "Success\n";

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

bool waitonexit = false;

int imain(int argc, char **argv)
{
	CommandLineInfo cmd = CommandLineInfo::parseCommandLine(argc, argv);
	bool madeConsole = false;

	BOOST_FOREACH(const CommandLineCommand& clc, cmd.list0) {
		if (clc[0] == "--console") {
			if (!platform::hasConsole()) madeConsole = platform::newConsole();
		} else if (clc[0] == "--sign") {
			return AutoUpdater::doSigning(clc[1], clc[2], clc[3], clc[4], clc[5]) ? 0 : 1;
		} else if (clc[0] == "--write-build-version") {
			std::ofstream ofs(argv[2]);
			ofs << get_build_string();
			return 0;
		} else if (clc[0] == "--runtest") {
			return runtest();
		} else if (clc[0] == "--replace") {
			return AutoUpdater::replace(clc[1], clc[2]);
		}
	}

	waitonexit = true;

	// initialize settings & gui
	UFTTSettingsRef settings(UFTTSettingsRef::create());
	if (!settings.load()) {
		// try legacy settings
		UFTTSettingsLegacyLoader(settings);
	};

	try {
		std::vector<char*> gargs;
		std::shared_ptr<UFTTGui> gui;
		UFTTCore core(settings, cmd);
		{
			BOOST_FOREACH(CommandLineCommand& clc, cmd.list2) {
				if (clc[0] == "--gui-opt" || clc[0] == "--gui-opts") {
					for (size_t i = 1; i < clc.size(); ++i) {
						clc[i] += '\x00';
						gargs.push_back(&clc[i][0]);
					}
				}
			}

			gui = UFTTGui::makeGui(gargs.size(), &gargs[0], settings);
		}
		std::cout << "Build: " << get_build_string() << '\n';

		core.initialize();
		gui->bindEvents(&core);

		core.handleArgs(&cmd);

		if (madeConsole)
			platform::freeConsole();

		int ret = gui->run();

		settings.save();

#ifndef DEBUG
		// TODO: figure out why ~UFTTCore hangs sometimes
		core.get_io_service().stop();
		gui.reset();
		if (ret == 0) platform::exit(0);
#endif
		return ret;
	} catch (int i) {
		return i;
	}
}

int main( int argc, char **argv ) {
	int ret = -1;
	string message;
	try {
		ret = imain(argc, argv);
	} catch (std::exception& e) {
		message = string(e.what());
	} catch (...) {
		message = "unknown exception";
	}
	if (message != "") {
		if (!platform::hasConsole()) platform::newConsole();
		printf("fatal: %s\n", message.c_str());
	}
	if (waitonexit && ret != 0 && platform::hasConsole()) {
		printf("program aborted, termination in 30 seconds.\n");
		platform::msSleep(30*1000);
	}
	return ret;
}
