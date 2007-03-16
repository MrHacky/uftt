#include "stdafx.h"
#include "sharelister_gui.h"
#include "servert.h"
#include "main.h"
#include "yarn.h"
#include "packet.h"
#include "sharelister.h"

using namespace std;

extern bool udp_hax;
SOCKET ServerSock;

int WINAPI ServerThread(bool * Restart) {
	fprintf(stderr,"Initializing servert\n");
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;


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
			switch ( recv_buf[0] ) { //first byte indicate packet type
				case PT_QUERY_SERVERS:
					fprintf( stderr, "PT_QUERY_SERVERS\n" );
					UFTT_packet reply;
					reply.type = PT_REPLY_SERVERS;
					send_packet(&reply, ServerSock);
					break;
				case PT_QUERY_SHARELIST:
					fprintf( stderr, "PT_QUERY_SHARELIST\n" );
					//TODO: SpawnThread
					fprintf(stderr,"Sending Sharelist\n");
					for(uint32 i = 0; i < myServer->share.size(); ++i) {
						fprintf(stderr, "Sending share #%i\n",i);
						reply_servers reply;
						reply.type = PT_REPLY_SHARELIST;
						reply.num_shares = myServer->share.size();
						reply.share_num  = i;
						reply.UID = myServer->share[i]->UID;
						reply.name = myServer->share[i]->name;
						send_packet((UFTT_packet*)&reply, ServerSock);
					}
					break;
				case PT_REPLY_SERVERS: {
					fprintf( stderr, "PT_REPLY_SERVERS\n" );
					ServerInfo* serv = new ServerInfo;
					serv->address = (sockaddr*)malloc(sizeof(sockaddr));
					memcpy(serv->address, &source_addr, sizeof(sockaddr));
					servers.push_back(serv);
					spawnThread(get_sharelist, serv);
				}; break;
				case PT_RESTART_SERVER:
					fprintf( stderr, "Received restart packet\n" );
					break;
				case PT_REPLY_SHARELIST: {
					fprintf( stderr, "PT_REPLY_SHARELIST\n" );
					reply_servers reply;
					uint8 * index = (uint8*)recv_buf+1;
					uint32 num,sum;
					uint64 UID;
					string name;
					deserialize(&index, sum);
					deserialize(&index, num);
					deserialize(&index, UID);
					deserialize(&index, name);
					fprintf( stderr, "%i/%i :  = %s\n", num+1, sum, name.c_str());
					/*ServerInfo* serv = new ServerInfo;
					serv->address = (sockaddr*)malloc(sizeof(sockaddr));
					memcpy(serv->address, &source_addr, sizeof(sockaddr));
					servers.push_back(serv);
					spawnThread(get_sharelist, serv);
					*/
				}; break;
				default:
					fprintf( stderr, "Unknown message!\n" );
					break;
			}
		}
	}
	fprintf( stderr, "Restarting server ...\n" );
	*Restart=false;
	closesocket(ServerSock);
	spawnThread( ServerThread,Restart );
	ThreadExit( 0 );
}
