#ifndef QT_MAIN_H
#define QT_MAIN_H

//#include "../network/NetworkThread.h"
class NetworkThread;

class QTMain {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain( int argc, char **argv );
		~QTMain();
		
		void BindEvents(NetworkThread* nwobj);
		
		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
