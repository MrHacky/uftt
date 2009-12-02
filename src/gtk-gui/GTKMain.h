#ifndef GTK_MAIN_H
#define GTK_MAIN_H


#include "../UFTTGui.h"
#include "../UFTTCore.h"
#include "../UFTTSettings.h"
#include <boost/shared_ptr.hpp>

class GTKMain : public UFTTGui {
	private:
		// implementation class (PIMPL idiom)
		boost::shared_ptr<class GTKImpl> impl;
		GTKMain();

	public:
		GTKMain(int argc, char** argv, UFTTSettingsRef settings);
		void bindEvents(UFTTCoreRef core);
		int run();
		~GTKMain();
};

#endif