//---------------------------------------------------------------------------//
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "crossnet.h"

#include <string>
#include <iostream>
#include <stdio.h>
#include <assert.h>



using namespace std;

#define BUFLEN 512

void show_menu(int port) {
	fprintf(stdout,"\
	.--------------------------------.\n\
	| 1) Send message                |\n\
	| 2) Receive message             |\n\
	| 3) Set port #  (Current=%05i) |\n\
	|                                |\n\
	| 4) Exit                        |\n\
	`--------------------------------'\n",port);
}

//typedef SOCKET int;
//const INVALID_SOCKET = -1;

SOCKET udp_sock;


bool init_stuff(int port)
{
	struct sockaddr_in sock_server;

#ifdef HAVE_WINSOCK
	cout << "Initing winsock...";
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			cout << "Failed!" "\n";
			return 1;
	}
	cout << "Success!" "\n";
#endif

	//	SOCKET ipx_sock = socket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX);
	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		fprintf(stdout,"Error: could not get socket!");
		return false;
	}

	sock_server.sin_family = AF_INET;
	sock_server.sin_port = htons(port);
	sock_server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(udp_sock, (struct sockaddr *) &sock_server, sizeof(sockaddr)) == SOCKET_ERROR) {
		fprintf(stdout, "Error bind()ing socket: %i!\n", NetGetLastError());
		closesocket(udp_sock);
		return false;
	}

	cout << "Enabling broadcasting..." ;
	unsigned long ul = 1;
	int retval = setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (char*)&ul, sizeof(ul));
	if (retval == SOCKET_ERROR) {
		cout << "Failed! :" << NetGetLastError() << "\n";
		return 1;
	};
	cout << "Success!" "\n";

}

bool recv_msg(string &msg, int port) {
		struct sockaddr_in sock_server;
		struct sockaddr_in sock_client;

		socklen_t socklen = sizeof(struct sockaddr_in);
		char buf[BUFLEN];

		if (recvfrom(udp_sock, buf, BUFLEN, 0, (struct sockaddr *) &sock_client, &socklen) == SOCKET_ERROR) {
			fprintf(stdout, "Error receiving msg!\n");
			return false;
		}

		printf("Received packet from %s:%d\n", inet_ntoa(sock_client.sin_addr), ntohs(sock_client.sin_port));

		msg = buf;
		return true;
}

bool send_msg(const string &msg, int port) {
		struct sockaddr_in sock_server;
		socklen_t socklen = sizeof(struct sockaddr_in);
		char buf[BUFLEN];

	sock_server.sin_family = AF_INET;
	sock_server.sin_port = htons(port);
	sock_server.sin_addr.s_addr = htonl(INADDR_BROADCAST);

		fprintf(stdout, "Sending packet\n");
		memset(buf, 0, BUFLEN);
		memcpy(buf, msg.c_str(), msg.size());
		if (sendto(udp_sock, buf, BUFLEN, 0, (struct sockaddr *) &sock_server, socklen) == SOCKET_ERROR) {
			fprintf(stdout,"Error sending packet %i\n", NetGetLastError());
			closesocket(udp_sock);
			return false;
		}
		return true;
}

int main(int argc, char* argv[]) {
	bool done=false;
	int port = 12345;
	char *buf= new char[256];
	string tmp;

	init_stuff(port);

	while(!done) {
		show_menu(port);
		switch(fgetc(stdin)) {
			case '1':
				fgets(buf, 256, stdin);
				send_msg((string)buf,port);
				break;
			case '2':
				recv_msg(tmp, port);
				fprintf(stdout,"Received msg: %s",tmp.c_str());
				break;
			case '3':
				fgets(buf, 256, stdin);
				port = atoi(buf);
				break;
			case '4':
				 done=true;
				break;
		}
	}
	return EXIT_SUCCESS;
}


/*/---------------------------------------------------------------------------

#pragma hdrstop
//#define WIN32

//extern "C" {

//}

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <string>

//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <windows.h>
//#include <Wsipx.h>

//extern "C" {

#		include <sys/socket.h>
#		include <netinet/in.h>
#		include <netinet/tcp.h>
#		include <arpa/inet.h>
#		include <net/if.h>
#	include <unistd.h>
#	include <sys/ioctl.h>

#include <netipx/ipx.h>
#include <net/ipx.h>
#include <linux/ipx.h>

//#	include <ipifcons.h>
//#	include <ipxconst.h>
//#	include <ipxrtdef.h>
//#	include <ipxtfflt.h>
//}

#include <errno.h>

#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#define WSAGetLastError() (errno)
//#define AF_IPX AF_NS

typedef struct addrinfo addrinfo;

using namespace std;

//---------------------------------------------------------------------------
SOCKET ipxsock;

int InitIPX()
{
#ifdef WIN32
	cout << "Initing winsock...";
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			cout << "Failed!" "\n";
			return 1;
	}
	cout << "Success!" "\n";
#endif

	cout << "Creating IPX socket...";
	ipxsock = socket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX);
	if (ipxsock == INVALID_SOCKET) {
		cout << "Failed! :" << WSAGetLastError() << "\n";
		return 1;
	};
	cout << "Success!" "\n";
	int retval;

	sockaddr target_addr;
	SOCKADDR_IPX* target = (SOCKADDR_IPX*)&target_addr;

	target->sa_family = AF_IPX;
	for (int i=0; i<4; ++i)
		target->sa_netnum[i] = 0;
	for (int i=0; i<6; ++i)
		target->sa_nodenum[i] = 0xff;

	cout << "Listen Port Number? >";
	cin >> target->sa_socket;

	if (target->sa_socket != 0) {
		cout << "Binding IPX socket...";
		retval = bind(ipxsock, &target_addr, sizeof(target_addr));
		if (retval == SOCKET_ERROR) {
			cout << "Failed! :" << WSAGetLastError() << "\n";
			return 1;
		};
		cout << "Success!" "\n";
	}

	cout << "Enabling broadcasting..." ;
	unsigned long ul = 1;
	retval = setsockopt(ipxsock, SOL_SOCKET, SO_BROADCAST, (char*)&ul, sizeof(ul));
	if (retval == SOCKET_ERROR) {
		cout << "Failed! :" << WSAGetLastError() << "\n";
		return 1;
	};
	cout << "Success!" "\n";

	return 0;
}

int IPXSend()
{
	sockaddr target_addr;
	SOCKADDR_IPX* target = (SOCKADDR_IPX*)&target_addr;

	target->sa_family = AF_IPX;
	for (int i=0; i<4; ++i)
		target->sa_netnum[i] = 0;
	for (int i=0; i<6; ++i)
		target->sa_nodenum[i] = 0xff;

	cout << "Send Port Number? >";
	cin >> target->sa_socket;

	char sbuf[]="123456789\0";

	int retval;
	cout << "Broadcasting test Packet...";
	retval = sendto(ipxsock, sbuf, 10, 0, &target_addr, sizeof(target_addr));

	if (retval == SOCKET_ERROR) {
		cout << "Failed! :" << WSAGetLastError() << "\n";
		return 1;
	};
	cout << "Success! :" << retval << "\n";
	return 0;

}

char* hexbyte(unsigned char x)
{
	char* rbuf = (char*)malloc(5); // leaks badly :O

	rbuf[1] = "0123456789ABCDEF"[(x >> 0) & 0x0F];
	rbuf[0] = "0123456789ABCDEF"[(x >> 4) & 0x0F];

	rbuf[2] = 0;
	return rbuf;
}

int IPXRecv()
{
	char sbuf[100];
	sockaddr source_addr;
	SOCKADDR_IPX* source = (SOCKADDR_IPX*)&source_addr;

	int len = sizeof(source_addr);
	int retval;
	cout << "Receiving test Packet...";
	retval = recvfrom(ipxsock, sbuf, 100, 0, &source_addr, &len);

	if (retval == SOCKET_ERROR) {
		cout << "Failed! :" << WSAGetLastError() << "\n";
		return 1;
	};
	cout << "Success! :" << sbuf << "\n";
	cout << "From: " <<
		hexbyte(source->sa_netnum[0])  << "-" <<
		hexbyte(source->sa_netnum[1])  << "-" <<
		hexbyte(source->sa_netnum[2])  << "-" <<
		hexbyte(source->sa_netnum[3])  << ":" <<
		hexbyte(source->sa_nodenum[0]) << "-" <<
		hexbyte(source->sa_nodenum[1]) << "-" <<
		hexbyte(source->sa_nodenum[2]) << "-" <<
		hexbyte(source->sa_nodenum[3]) << "-" <<
		hexbyte(source->sa_nodenum[4]) << "-" <<
		hexbyte(source->sa_nodenum[5]) << ":" <<
		source->sa_socket;
	return 0;
}

#pragma argsused

int main(int argc, char* argv[])
{
	// main menu
	bool done = false;
	while (!done) {
		cout << "\n" "--- UFTT Main Menu ---" "\n";
		cout << "1. Init IPX" "\n";
		cout << "2. Send IPX Broadcast" "\n";
		cout << "3. Receive IPX Packets" "\n";

		cout << "enter a number or 'q' to quit" "\n";
		cout << ">";

		char ch;
		cin >> ch;

		switch (ch) {
			case '1': {
				InitIPX();
			}; break;
			case '2': {
				IPXSend();
			}; break;
			case '3': {
				IPXRecv();
			}; break;
			case 'q': {
				done = true;
			}; break;
		};
	};
	return 0;
}
//---------------------------------------------------------------------------
*/


