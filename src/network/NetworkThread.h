#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include <iostream>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>

#include "../sha1/SHA1.h"

class NetworkThread {
	public:
		// TODO: use signals for callbacks?
		boost::function<void()> cbAddServer;
		boost::function<void(std::string, SHA1)> cbAddShare;

		void operator()();
};

#endif
