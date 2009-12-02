#ifndef QT_MAIN_H
#define QT_MAIN_H

#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>

#include "../UFTTCore.h"
class UFTTCore;
class UFTTSettings;
class QTMain {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain( int& argc, char **argv, UFTTSettings* settings);
		~QTMain();

		void bindEvents(UFTTCore* t);
		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
