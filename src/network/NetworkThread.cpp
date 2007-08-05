#include "NetworkThread.h"

#include <iostream>

#include <boost/foreach.hpp>

#include "../SharedData.h"
#include "CrossPlatform.h"
#include "Packet.h"

using namespace std;

#define SERVER_PORT 12345

static string addr2str( sockaddr* addr ) {
	char buf[100];
	switch ( addr->sa_family ) {
		case AF_IPX: {
			sockaddr_ipx* ipx_addr = ( sockaddr_ipx* )addr;
			snprintf( buf, 100, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
								(( uint8* )( &ipx_addr->sa_netnum ) )[0],
								(( uint8* )( &ipx_addr->sa_netnum ) )[1],
								(( uint8* )( &ipx_addr->sa_netnum ) )[2],
								(( uint8* )( &ipx_addr->sa_netnum ) )[3],
								( uint8 )ipx_addr->sa_nodenum[0], ( uint8 )ipx_addr->sa_nodenum[1],
								( uint8 )ipx_addr->sa_nodenum[2], ( uint8 )ipx_addr->sa_nodenum[3],
								( uint8 )ipx_addr->sa_nodenum[4], ( uint8 )ipx_addr->sa_nodenum[5],
								ntohs( ipx_addr->sa_socket ) );
		}
		; break;
		case AF_INET: {
			sockaddr_in* ip_addr = ( sockaddr_in* )addr;
			snprintf( buf, 100, "%u.%u.%u.%u:%i",
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[0],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[1],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[2],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[3],
								ntohs( ip_addr->sin_port ) );
		}
		; break;
		default:
			snprintf( buf, 100, "Unknown address family: %i", addr->sa_family);
	}
	return string( buf );
}

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
	UFTT_packet rpacket;
	UFTT_packet spacket;
	sockaddr source_addr;
	vector<JobRequest> MyJobs;

	udpsock = CreateUDPSocket(SERVER_PORT, NULL);
	assert(udpsock != INVALID_SOCKET);

	// initialise networkerrno 97
	while (!terminating) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(udpsock, &readset);

		// poll for incoming stuff
		int sel = select(udpsock+1, &readset, NULL, NULL, &tv);
		//cout << "sel:" << sel << '\n';
		socklen_t len = sizeof(source_addr);
		assert(sel >= 0);
		if (FD_ISSET(udpsock, &readset)) {
			if (recvfrom(udpsock, rpacket.data, 1400, 0, &source_addr, &len) == SOCKET_ERROR) {
				fprintf(stderr, "Server: recvfrom() failed with error #%i\n", NetGetLastError());
			} else {
				rpacket.curpos = 0;
				uint8 ptype;
				rpacket.deserialize(ptype);
				cout << "got packet, type:" << (int)ptype << endl;
				cout << "from:" << addr2str(&source_addr) << endl;
				switch (ptype) {
					case PT_QUERY_SERVERS: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_SERVERS);

						sockaddr_in* udp_addr = ( sockaddr_in * )&source_addr;

						udp_addr->sin_family = AF_INET;

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					};
					case PT_REPLY_SERVERS: {
						cout << "TODO: post GUI event for new server" << endl;
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_QUERY_SHARELIST);

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					};
					case PT_QUERY_SHARELIST: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_SHARELIST);

						{
							boost::mutex::scoped_lock lock(shares_mutex);
							spacket.serialize<uint32>(MyShares.size());
							BOOST_FOREACH(const ShareInfo & si, MyShares) {
								spacket.serialize(si.root->name);
								BOOST_FOREACH(uint8 val, si.root->hash.data)
									spacket.serialize(val);
							}
						}
					};
				}
			}
		}

		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			for (; JobQueue.size() > 0; JobQueue.pop_back()) {
				const JobRequest& job = JobQueue.back();
				MyJobs.push_back(job);
			}
		}

		for (; MyJobs.size() > 0; MyJobs.pop_back()) {
			const JobRequest& job = MyJobs.back();
			switch (job.type) {
				case PT_QUERY_SERVERS: {
					spacket.curpos = 0;
					spacket.serialize<uint8>(PT_QUERY_SERVERS);
					
					sockaddr target_addr;
					sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;
					
					udp_addr->sin_family = AF_INET;
					udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
					udp_addr->sin_port = htons( SERVER_PORT );

					if (sendto(udpsock, spacket.data, spacket.curpos, 0, &target_addr, sizeof( target_addr ) ) == SOCKET_ERROR)
						cout << "error sending packet: " << NetGetLastError() << endl;
					break;
				}
				default: {
					cout << "unknown job type: " << (int)job.type << endl;
				}
			}
		}

	}
}
