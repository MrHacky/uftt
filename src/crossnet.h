#ifndef CROSSNET_H
#define CROSSNET_H

#if defined(__WIN32__)
// borland win32 specifics..
#define HAVE_WINSOCK
#endif

#if defined(HAVE_WINSOCK)

#include <winsock2.h>
#include <Wsipx.h>
#include <ws2tcpip.h>

#define NetGetLastError() WSAGetLastError()

#else
// non-win32 (unix?) stuff

#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h> // for errno in NetGetLastError() macro
#endif

#define closesocket close
#define NetGetLastError() errno

typedef int SOCKET;

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)


#endif



#endif //CROSSNET_H
