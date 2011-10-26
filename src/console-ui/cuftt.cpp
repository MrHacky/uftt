#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include "../UFTTGui.h"
#include "../UFTTCore.h"

#include <iostream>

struct servicelocker {
	boost::mutex mutex;
	boost::asio::io_service* service;
};

class ConsoleMain : public UFTTGui {
	public:
		boost::asio::io_service service;
		boost::shared_ptr<boost::asio::io_service::work> work;
		boost::shared_ptr<servicelocker> lockedservice;

		ConsoleMain(int argc, char **argv, UFTTSettingsRef settings)
		: work(new boost::asio::io_service::work(service))
		, lockedservice(new servicelocker())
		{
			lockedservice->service = &service;
		}

		~ConsoleMain()
		{
		}

		void bindEvents(UFTTCore* core)
		{
			core->connectSigGuiCommand(boost::bind(&ConsoleMain::handleGuiCommand, this, _1));
		}

		void handleGuiCommand(UFTTCore::GuiCommand cmd)
		{
			if (cmd == UFTTCore::GUI_CMD_QUIT) {
				work.reset();
			}
		}

		void handleInput(const std::string& line)
		{
			work.reset();
		}

		int run()
		{
			boost::thread inputthread(boost::bind(&ConsoleMain::input, this, lockedservice));
			service.run();
			boost::mutex::scoped_lock lock(lockedservice->mutex);
			lockedservice->service = NULL;
			return 0;
		}

		static void input(ConsoleMain* cmain, boost::shared_ptr<servicelocker> lockedservice)
		{
			std::cout << "Press enter to exit\n";
			std::string line;
			while (std::getline(std::cin, line)) {
				boost::mutex::scoped_lock lock(lockedservice->mutex);
				if (lockedservice->service)
					lockedservice->service->post(boost::bind(&ConsoleMain::handleInput, cmain, line));
				else
					return;
			}
		}
};

const boost::shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	return boost::shared_ptr<UFTTGui>(new ConsoleMain(argc, argv, settings));
}
