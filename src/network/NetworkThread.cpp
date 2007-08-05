#include "NetworkThread.h"

#include <iostream>

#include <boost/foreach.hpp>

#include "../SharedData.h"
#include "CrossPlatform.h"
#include "Packet.h"

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
/*
	// now enable nonblocking
	unsigned long blk = 1;
	retval = setsockopt( sock, SOL_SOCKET, SO_BROADCAST, ( char* )&blk, sizeof( blk ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "UDP: Unable to enable nonblocking:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	};
*/
	return sock;
}

void NetworkThread::operator()()
{
	SOCKET udpsock;
	UFTT_packet rpacket;
	UFTT_packet spacket;
	sockaddr source_addr;

	udpsock = CreateUDPSocket(12345, NULL);
	assert(udpsock != INVALID_SOCKET);


	// initialise network
	while (!terminating) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(udpsock, &readset);
		
		int sel = select(1, &readset, NULL, NULL, &tv);
		cout << "sel:" << sel << '\n';
		socklen_t len;
		assert(sel >= 0);
		if (FD_ISSET(udpsock, &readset)) {
			if (recvfrom(udpsock, rpacket.data, 1400, 0, &source_addr, &len) == SOCKET_ERROR) {
				fprintf(stderr, "Server: recvfrom() failed with error #%i\n",NetGetLastError());
			} else {
				rpacket.curpos = 0;
				uint8 curtype;
				rpacket.deserialize(curtype);
			}
		}
		
		// poll for incoming stuff
		// poll for outgoing stuff
		cerr << '.' << endl;

		{
			boost::mutex::scoped_lock lock(shares_mutex);
			BOOST_FOREACH(const ShareInfo& si, MyShares) {
				cerr << si.root->name << ':' << si.root->size << endl;
			}
		}

	}
}
