#ifndef GTK_MAIN_H
#define GTK_MAIN_H


#include "../UFTTGui.h"
#include "../UFTTCore.h"
#include "../UFTTSettings.h"
#include <memory>

class GTKMain : public UFTTGui {
	private:
		// implementation class (PIMPL idiom)
		std::shared_ptr<class GTKImpl> impl;
		GTKMain();

	public:
		GTKMain(int argc, char** argv, UFTTSettingsRef settings);
		void bindEvents(UFTTCore* core);
		int run();
		~GTKMain();
};

#endif
