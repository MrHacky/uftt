//---------------------------------------------------------------------------

#pragma hdrstop

//#define WIN32

//extern "C" {

//}

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <Wsipx.h>


//extern "C" {
/*
#		include <sys/socket.h>
#		include <netinet/in.h>
#		include <netinet/tcp.h>
#		include <arpa/inet.h>
#		include <net/if.h>
#include <nspapi.h>
#	include <unistd.h>
#	include <sys/ioctl.h>
*/
//}

//#	define SOCKET int
//#	define INVALID_SOCKET -1
//#		define WSAGetLastError() (errno)

typedef struct addrinfo addrinfo;

using namespace std;

//---------------------------------------------------------------------------
SOCKET ipxsock;

int InitIPX()
{
#if defined(WIN32)
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



