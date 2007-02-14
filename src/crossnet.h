#ifndef CROSSNET_H
#define CROSSNET_H

#ifdef WIN32
// win32 specific stuff
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <windows.h>
#	include <Wsipx.h>

#define NetGetLastError() WSAGetLastError()

#else
// non-win32 (unix?) stuff
#include <unistd.h>
//#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include <errno.h> // for errno in NetGetLastError() macro

#define closesocket close
#define NetGetLastError() errno

typedef int SOCKET;

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)


#endif


#endif //CROSSNET_H
