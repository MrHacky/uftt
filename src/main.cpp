//---------------------------------------------------------------------------//
#include "stdafx.h"

#include "main.h"
#include "yarn.h"
#include "gooey.h"
#include "servert.h"
#include "sharelister.h"
#include "packet.h"
using namespace std;

bool ServerRestart = false;

THREAD ServertThread;

#define BUFLEN 512

vector<sockaddr_ipx> IPXInterfaces;

/**
 * Creates an IPX socket and returns the handle
 * enables broadcasting, binds to bindport if != 0
 */
SOCKET CreateIPXSocket( uint16 bindport, sockaddr_ipx* iface_addr ) {
	sockaddr_ipx addr;
	SOCKET sock;
	int retval;

	sock = socket( AF_IPX, SOCK_DGRAM, NSPROTO_IPX );
	if ( sock == INVALID_SOCKET ) {
		cout << "IPX: Failed to create socket: " << NetGetLastError() << "\n";
		return INVALID_SOCKET;
	}

	addr.sipx_family = AF_IPX;
	for ( int i=0; i<4; ++i )
		(( char* )( &addr.sa_netnum ) )[i] = 0; // cast for compatibility when netnum is an int
	for ( int i=0; i<6; ++i )
		addr.sa_nodenum[i] = 0;

#ifdef HAVE_WINSOCK
	addr.sa_socket = htons( bindport );
#else
	// in linux binding to a port fails if netnum isnt specified
	// so we first bind to any port to find out the netnum
	addr.sa_socket = 0;
#endif

	if ( iface_addr != NULL ) {
		addr.sipx_family = AF_IPX;
		memcpy( &addr, iface_addr, sizeof( addr ) );
		addr.sa_socket = htons( bindport );
		cout << "Binding to:" << addr2str(( sockaddr* )&addr ) << "\n";
		bindport = 0;
	}

	retval = bind( sock, ( sockaddr* )&addr, sizeof( addr ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "IPX: Bind to network failed:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	}

	// now we query for the netnum and use that to bind with the correct port
#ifndef HAVE_WINSOCK
	if ( bindport != 0 ) {
		socklen_t len = sizeof( addr );
		retval = getsockname( sock, ( sockaddr* )&addr, &len );
		if ( retval == SOCKET_ERROR ) {
			cout << "IPX: Failed to retrieve network:" << NetGetLastError() << "\n";
			closesocket( sock );
			return INVALID_SOCKET;
		}

		// bind-ing twice on the same socket fails... so create a new one
		closesocket( sock );
		sock = socket( AF_IPX, SOCK_DGRAM, NSPROTO_IPX );
		if ( sock == INVALID_SOCKET ) {
			cout << "IPX: Failed to create new socket: " << NetGetLastError() << "\n";
			return INVALID_SOCKET;
		}

		addr.sa_socket = htons( bindport );

		retval = bind( sock, ( sockaddr* )&addr, sizeof( addr ) );
		if ( retval == SOCKET_ERROR ) {
			cout << "IPX: Bind to port failed:" << NetGetLastError() << "\n";
			closesocket( sock );
			return INVALID_SOCKET;
		}
	}
#endif

	// now enable broadcasting
	unsigned long bc = 1;
	retval = setsockopt( sock, SOL_SOCKET, SO_BROADCAST, ( char* )&bc, sizeof( bc ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "IPX: Unable to enable broadcasting:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	};
	return sock;
}

SOCKET CreateUDPSocket( uint16 bindport, sockaddr_in* iface_addr) {
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

string addr2str( sockaddr* addr ) {
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
			return string( "Unknown address family" );
	}
	return string( buf );
}

void ListIPXInterfaces() {
	IPXInterfaces.clear();
#if defined(HAVE_WINSOCK) && defined(HAVE_WSNWLINK_H)
// this method seems to be winsock-only
	sockaddr_ipx ipx_sa;
	IPX_ADDRESS_DATA ipx_data;
	int retval, cb, nAdapters, i=0;
	SOCKET tempsock = CreateIPXSocket( 0 );

	cb = sizeof( nAdapters );
	retval = getsockopt( tempsock,
											 NSPROTO_IPX,
											 IPX_MAX_ADAPTER_NUM,
											 ( CHAR * ) &nAdapters,
											 &cb
										 );

	if ( retval == SOCKET_ERROR ) {
		cout << "EnumerateAdapters " << "getsockopt " << NetGetLastError( ) << "\n";
		return;
	}

	fprintf( stdout, "Total number of adapters -> %d\n", nAdapters );

	while ( nAdapters > 0 ) {
		memset( &ipx_data, 0, sizeof( ipx_data ) );
		ipx_data.adapternum = ( nAdapters -1 );
		cb = sizeof( ipx_data );

		retval = getsockopt( tempsock,
												 NSPROTO_IPX,
												 IPX_ADDRESS,
												 ( CHAR * ) &ipx_data,
												 &cb
											 );

		if ( SOCKET_ERROR == retval ) {
			cout << "EnumerateAdapters " << "getsockopt " << NetGetLastError( ) << "\n";
		}

		//
		// Print each address
		//
		memcpy( &ipx_sa.sa_netnum, &ipx_data.netnum, 4 );
		memcpy( &ipx_sa.sa_nodenum, &ipx_data.nodenum, 6 );
		ipx_sa.sa_socket = 0;
		ipx_sa.sa_family = AF_IPX;
		cout << " : " << addr2str(( sockaddr* )&ipx_sa ) << " wan:" <<ipx_data.wan << " status:"<<ipx_data.status
		<< " psize:"<<ipx_data.maxpkt<<" speed:"<<ipx_data.linkspeed<< "\n";
		if ( ipx_data.status ) {
			IPXInterfaces.push_back( ipx_sa );
		}
		nAdapters--;
	}

	return;
#else
	// TODO: read /proc/net/ipx/interface and/or /proc/net/ipx_interface
#endif
}

void ListUDPInterfaces() {}

void show_menu( int port ) {
	fprintf( stdout,"\
					 .--------------------------------.\n\
					 | 1) Send message                |\n\
					 | 2) Receive message             |\n\
					 | 3) Set port #  (Current=%05i) |\n\
					 | 4) Set ipx interface           |\n\
					 | 5) send Broadcast loop         |\n\
					 | 6) switch to UDP               |\n\
					 | 7) receive loop                |\n\
					 |                                |\n\
					 | q) Exit                        |\n\
					 `--------------------------------'\n",port );
}

SOCKET ipx_sock;

bool init_stuff() {
#ifdef HAVE_WINSOCK
	WSAData wsaData;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		cout << "Failed to initilize winsock" "\n";
		return 1;
	}
#endif
	return 0;
}

int recv_msg( string &msg, int port, string* from = NULL ) {
	char buf[1500];
	sockaddr source_addr;

	socklen_t len = sizeof( source_addr );
	int retval;
	retval = recvfrom( ipx_sock, buf, 1400, 0, &source_addr, &len );
	if ( retval == SOCKET_ERROR ) {
		cout << "Failed! :" << NetGetLastError() << "\n";
		return 0;
	};

	if ( from != NULL ) *from = addr2str( &source_addr );

	msg = buf;
	return retval;
}

bool udp_hax = false;

int send_msg( const string &msg, int port ) {
	sockaddr target_addr;
	sockaddr_ipx* ipx_addr =( sockaddr_ipx* )&target_addr;
	sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;

	ipx_addr->sipx_family = AF_IPX;
	for ( int i=0; i<4; ++i )
		(( uint8* )&ipx_addr->sa_netnum )[i] = 0;
	for ( int i=0; i<6; ++i )
		ipx_addr->sa_nodenum[i] = 0xFF;

	ipx_addr->sa_socket = htons( port );

	// UDP:
	if ( udp_hax ) {
		udp_addr->sin_family = AF_INET;
		udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
		udp_addr->sin_port = htons( port );
	}

// cout << "Got Address: " << addr2str(&target_addr) << "\n";
	char mbuf[1500];

	memcpy( mbuf, msg.c_str(), msg.size()+1 );

	int retval;
// cout << "Broadcasting test Packet...";
	retval = sendto( ipx_sock, mbuf, 1400, 0, (const sockaddr*)&target_addr, sizeof( target_addr ) );

	if ( retval == SOCKET_ERROR ) {
		cout << "Failed! :" << NetGetLastError() << "\n";
		return 0;
	};
// cout << "Success! :" << retval << "\n";
	return retval;
}

int port = 54321; // increased cause ports <~15000 are reserved for root in linux
SOCKET UsePort( int p ) {
	closesocket( ipx_sock );
	ipx_sock = CreateIPXSocket( p );
	port = p;
	return ipx_sock;
}

SOCKET UseInterface( int i ) {
	closesocket( ipx_sock );
	ipx_sock = CreateIPXSocket( port, &IPXInterfaces[i] );
	return ipx_sock;
}

SOCKET UseUDP( bool b ) {
	ServerRestart = true;
	UFTT_packet restart;
	restart.type = PT_RESTART_SERVER;
	send_packet(&restart, ipx_sock);
	closesocket( ipx_sock );
	if ( b ) ipx_sock = CreateUDPSocket( port );
	else  ipx_sock = CreateIPXSocket( port );
	udp_hax = b;
	return ipx_sock;
}

bool SpamMoreSpam=false;
int WINAPI SpamSpam( SpamSpamArgs *Args ) {
	if ( !SpamMoreSpam ) {
		SpamMoreSpam=true;
		uint32 c1=0, c2 = 0;
		while ( SpamMoreSpam ) {
			c1 += send_msg( Args->s,port );
			if ( c1 > 1024*1024 ) {
				c2 += c1;
				c1 -= 1024*1024;
				( Args->p )( c2 );
			}
		}
		delete Args;
	}
	else {
		cout << "\nWarning: already spamming Spam!\n";
	}
	ThreadExit( NULL ); //does not return
}

bool ReceiveMoreSpam=false;
int WINAPI ReceiveSpam( SpamSpamArgs *Args ) {
	if ( !ReceiveMoreSpam ) {
		ReceiveMoreSpam=true;
		uint32 c1=0, c2 = 0;
		string tmp="";
		while ( ReceiveMoreSpam ) {
			c1 += recv_msg( tmp, port );
			if ( c1 > 1024*1024 ) {
				c2 += c1;
				c1 -= 1024*1024;
				( Args->p )( c2 );
			}
		}
		delete Args;
	}
	else {
		cout << "\nWarning: already receiving Spam!\n";
	}
	ThreadExit( 0 ); //does not return
}

int main( int argc, char* argv[] ) {
	init_stuff();
	if (( ipx_sock = CreateIPXSocket( port ) )==INVALID_SOCKET ) {
		if(( ipx_sock = CreateUDPSocket( port ))==INVALID_SOCKET ) {
			fprintf( stderr,"\
							Make sure that you have an IPX or UDP interface set up.\n\
							For example to set up IPX try running something like:\n\
							ipx_interface add -p eth0 802.2 && ipx_configure --auto_primary=on --auto_interface=on\n\
							");
			return EXIT_FAILURE;
			}
		else {
		  udp_hax = true;
		  //FIXME: GUI BUG IPX/UDP Radiobutton
		  fprintf(stderr,"FIXME: GUI BUG IPX/UDP Radiobutton is now b0rken\n");
		}
	}
	init_server_list(  );
	ServertThread = spawnThread(ServerThread, &ServerRestart);

	/* #ifdef USE_GUI */
	gtk_init (&argc, &argv);
	return show_gooey();
}

#ifdef WIN32
int __stdcall  WinMain(HINSTANCE hinstance, HINSTANCE previnstanc, LPSTR showcommand, int nshow)
{
	main(0, NULL);
}
#endif

