#include "NetworkThread.h"

#include <iostream>

#include <boost/foreach.hpp>

#include "SharedData.h"

using namespace std;

void NetworkThread::operator()()
{
	// initialise network
	while (true) {
		// poll for incoming stuff
		// poll for outgoing stuff
		cerr << '.' << endl;

		{
			boost::mutex::scoped_lock lock(shares_mutex);
			BOOST_FOREACH(const ShareInfo& si, MyShares) {
				cerr << si.root->name << ':' << si.root->size << endl;
			}
		}

		usleep(200000);
	}
}
