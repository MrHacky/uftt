#ifndef QT_MAIN_H
#define QT_MAIN_H

//#include "../network/NetworkThread.h"
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>

class SimpleBackend;
class QTMain {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain( int& argc, char **argv );
		~QTMain();

		void BindEvents(SimpleBackend* t);

		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
