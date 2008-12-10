#ifndef QT_MAIN_H
#define QT_MAIN_H

//#include "../network/NetworkThread.h"
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>

#include "../IBackend.h"

class UFTTSettings;
class QTMain {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain( int& argc, char **argv, UFTTSettings* settings);
		~QTMain();

		void BindEvents(IBackend* t);

		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
