#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>
#include <memory>

#include "../UFTTGui.h"
#include "../UFTTCore.h"

#include "../util/StrFormat.h"

#include <iostream>
#include <set>

struct servicelocker {
	boost::mutex mutex;
	boost::asio::io_service* service;
};

class ConsoleMain : public UFTTGui {
	public:
		boost::asio::io_service service;
		std::shared_ptr<boost::asio::io_service::work> work;
		std::shared_ptr<servicelocker> lockedservice;

		UFTTCore* core;
		std::map<std::string, ShareID> sharemap;
		std::set<ShareID> shareset;

		ConsoleMain(int argc, char **argv, UFTTSettingsRef settings)
		: work(new boost::asio::io_service::work(service))
		, lockedservice(new servicelocker())
		{
			lockedservice->service = &service;
		}

		~ConsoleMain()
		{
		}

		void bindEvents(UFTTCore* core_)
		{
			core = core_;
			core->connectSigGuiCommand(boost::bind(&ConsoleMain::handleGuiCommand, this, _1));
			core->connectSigAddShare(service.wrap(boost::bind(&ConsoleMain::addShare, this, _1)));
		}

		void handleGuiCommand(UFTTCore::GuiCommand cmd)
		{
			if (cmd == UFTTCore::GUI_CMD_QUIT) {
				work.reset();
			}
		}

		void addShare(const ShareInfo& info)
		{
			if (shareset.count(info.id)) return;
			std::string key = STRFORMAT("%d", shareset.size());
			if (info.isupdate) return;
			std::cout << key << ": " << info.name << "\t(" << info.host << ", " << info.user << ")\n";
			sharemap[key] = info.id;
			shareset.insert(info.id);
		}

		void handleInput(const std::string& line)
		{
			std::vector<std::string> items;
			boost::split(items, line, boost::is_any_of(" \t"));
			if (line == "" || items.empty() || items[0] == "q" || items[0] == "quit")
				work.reset();
			else if (items[0] == "l" || items[0] == "list")
				refresh();
			else if ((items[0] == "d" || items[0] == "download") && items.size() == 3)
				core->startDownload(sharemap[items[1]], items[2]);
			else if ((items[0] == "s" || items[0] == "share") && items.size() == 2)
				core->addLocalShare(items[1]);
			else
				std::cout << "[q]uit, [l]ist, [d]ownload <id> <path>, [s]hare <path>, [r]emove <id> [h]elp\n";
		}

		void refresh()
		{
			std::cout << "Refresh!\n";
			sharemap.clear();
			shareset.clear();
			core->doRefreshShares();
		}

		int run()
		{
			std::cout << "Press enter to exit\n";
			boost::thread inputthread(boost::bind(&ConsoleMain::input, this, lockedservice));
			service.run();
			boost::mutex::scoped_lock lock(lockedservice->mutex);
			lockedservice->service = NULL;
			return 0;
		}

		static void input(ConsoleMain* cmain, std::shared_ptr<servicelocker> lockedservice)
		{
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

const std::shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	return std::shared_ptr<UFTTGui>(new ConsoleMain(argc, argv, settings));
}
