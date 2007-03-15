#include "sharelister_gui.h"
#include "servert.h"
#include "main.h"
#include "yarn.h"
extern bool udp_hax;

int WINAPI ServerThread(bool * Restart) {
	/*FIXME: These #defines need to be user configurable at runtime*/
	//-------------------------------------------------------------//
#define SERVER_PORT 55555
#define RECV_BUFFER_SIZE 1500
	//-------------------------------------------------------------//
	SOCKET ServerSock;

	if(udp_hax) {
		ServerSock = CreateUDPSocket(SERVER_PORT);
		if(ServerSock == INVALID_SOCKET) {
			fprintf(stderr, "Error creating UDP Socket, server thread exiting.\n");
			ThreadExit(-1);
		}
	}
	else {
		ServerSock = CreateIPXSocket(SERVER_PORT);
		if(ServerSock == INVALID_SOCKET) {
			fprintf(stderr, "Error creating IPX Socket, server thread exiting.\n");
			ThreadExit(-1);
		}
	}
	char recv_buf[RECV_BUFFER_SIZE];
	sockaddr source_addr;
	socklen_t len = sizeof(source_addr);
	while(!*Restart) {
		if (recvfrom(ServerSock, recv_buf, 1400, 0, &source_addr, &len) == SOCKET_ERROR) {
			fprintf(stderr, "Server: recvfrom() failed with error #%i\n",NetGetLastError());
		}
		else {
			fprintf( stderr,"Received a message from %s:\n",addr2str( &source_addr ).c_str() );
			switch ( recv_buf[0] ) {
				case 0x00:
					fprintf( stderr, "TEST\n" );
					break;
				case 0x42:
					fprintf( stderr, "42\n" );
					break;
				default:
					fprintf( stderr, "Unknown message!" );
					break;
			}
		}
	}
	fprintf( stderr, "Restarting server ...\n" );
	*Restart=false;
	spawnThread( ServerThread,Restart );
	ThreadExit( 0 );
}
