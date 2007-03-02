dnl Process this file with autoconf to produce a configure script.
AC_INIT(uftt, V0.0.1, simon.sasburg@gmail.com)
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_INSTALL
AC_STDC_HEADERS



PKG_CHECK_MODULES(DEPS, gtk+-2.0 >= 2.2 glib-2.0 >= 2.2 libglade-2.0 >= 2.5)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([netipx/ipx.h])
AC_CHECK_HEADERS([wsnwlink.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([dirent.h])

dnl Check for ipx availibility in BDS sockets
AC_CHECK_DECL(AF_IPX, [bsdsockipx=yes], [bsdsockipx=no],
[#if HAVE_NETINET_IN_H
# include <netinet/in.h>
# endif
])

dnl Try to use winsock if no ipx in BSD sockets
if test x$bsdsockipx = xno; then
	my_old_LIBS=$LIBS
	LIBS="-lws2_32 $LIBS"
	ws2_res=yes
	AC_MSG_CHECKING([for winsock])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <winsock2.h>]],
                                [[WSAStartup (0, NULL);]])],[
               AC_DEFINE(HAVE_WINSOCK, [], [Define if linking with winsock])
               AC_DEFINE(__USE_W32_SOCKETS, [], [Define if linking with winsock])
               ], [LIBS=$my_ac_save_LIBS ws2_res=no])
	AC_MSG_RESULT([$ws2_res])
fi


AC_OUTPUT(Makefile src/Makefile)
