#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include <iostream>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

class NetworkThread {
	public:
		void operator()();
};

#endif
