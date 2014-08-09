#ifndef QT_MAIN_H
#define QT_MAIN_H

#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include "../UFTTGui.h"
#include "../UFTTCore.h"
class UFTTCore;
class UFTTSettings;
class QTMain : public UFTTGui {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain(int argc, char **argv, UFTTSettingsRef settings);
		~QTMain();

		void bindEvents(UFTTCore* core);
		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
