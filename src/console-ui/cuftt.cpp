#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include "../UFTTGui.h"
#include "../UFTTCore.h"

#include <iostream>

class ConsoleMain : public UFTTGui {
	public:
		ConsoleMain(int argc, char **argv, UFTTSettingsRef settings)
		{
		}

		~ConsoleMain()
		{
		}

		void bindEvents(UFTTCore* core)
		{
		}

		int run()
		{
			std::cout << "Press enter to exit\n";
			std::string s;
			std::cin >> std::noskipws >> s;
			return 0;
		}
};

const boost::shared_ptr<UFTTGui> UFTTGui::makeGui(int argc, char** argv, UFTTSettingsRef settings) {
	return boost::shared_ptr<UFTTGui>(new ConsoleMain(argc, argv, settings));
}
