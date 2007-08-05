#include "NetworkThread.h"

#include <iostream>

#include <boost/foreach.hpp>

#include "SharedData.h"
#include "CrossPlatform.h"

using namespace std;

static SOCKET CreateUDPSocket( uint16 bindport, sockaddr_in* iface_addr) {
	sockaddr_in addr;
	SOCKET sock;
	int retval;

	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( sock == INVALID_SOCKET ) {
		cout << "UDP: Failed to create socket: " << NetGetLastError() << "\n";
		return INVALID_SOCKET;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( bindport );

	retval = bind( sock, ( sockaddr* )&addr, sizeof( addr ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "UDP: Bind to network failed:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	}

	// now enable broadcasting
	unsigned long bc = 1;
	retval = setsockopt( sock, SOL_SOCKET, SO_BROADCAST, ( char* )&bc, sizeof( bc ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "UDP: Unable to enable broadcasting:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	};
	return sock;
}


void NetworkThread::operator()()
{
	SOCKET udpsock;
	udpsock = CreateUDPSocket(12345, NULL);

	assert(udpsock != INVALID_SOCKET);

	// initialise network
	while (!terminating) {
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
