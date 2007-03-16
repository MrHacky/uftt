#ifndef MAIN_H
  #define MAIN_H

  #include "stdafx.h"

  int send_msg(const std::string& msg, int port);
  int recv_msg(std::string &msg, int port, std::string* from);
  SOCKET UsePort(int p);
  SOCKET UseInterface(int i);
  SOCKET UseUDP(bool b);
  struct SpamSpamArgs {
    std::string s;
    void (*p)(uint32);
  };
  int WINAPI SpamSpam(SpamSpamArgs *Args);
  int WINAPI ReceiveSpam(SpamSpamArgs *Args);
  int WINAPI ServerThread(bool * Restart);
  std::string addr2str( sockaddr* addr );
  SOCKET CreateIPXSocket( uint16 bindport, sockaddr_ipx* iface_addr = NULL );
  SOCKET CreateUDPSocket( uint16 bindport, sockaddr_in* iface_addr = NULL ) ;

  extern bool udp_hax;
#endif
