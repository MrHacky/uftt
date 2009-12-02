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
		GTKMain(int argc, char** argv, UFTTCore& core, UFTTSettings& settings);

	public:
		static const boost::shared_ptr<UFTTGui> makeGui(int argc, char** argv, UFTTCore& core, UFTTSettings& settings);
		void bindEvents(UFTTCore* t);
		int run();
		~GTKMain();
};

#endif