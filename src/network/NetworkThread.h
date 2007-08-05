#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include <iostream>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>

class NetworkThread {
	public:
		boost::function<void()> cbAddServer;
		void operator()();
};

#endif
