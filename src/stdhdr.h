#ifndef STDHDR_H
#define STDHDR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#elif defined(__WIN32__)
// borland win32 specifics..
#define HAVE_WINSOCK
#endif

#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif

#if defined(HAVE_WINSOCK)

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Wsipx.h>
#include <ws2tcpip.h>

#define NetGetLastError() WSAGetLastError()

#define sipx_family sa_family	

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

/* ripped from google code search somewhere */
#ifdef HAVE_NETIPX_IPX_H
# include <netipx/ipx.h>
#else
# include <linux/ipx.h>
# ifndef IPX_TYPE
#  define IPX_TYPE 1
# endif
#endif

#define closesocket close
#define NetGetLastError() errno

typedef int SOCKET;

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#define NSPROTO_IPX PF_IPX
#define SOCKADDR_IPX sockaddr_ipx
#define sa_nodenum sipx_node
#define sa_socket sipx_port
#define sa_netnum sipx_network
#endif



#endif //STDHDR_H
