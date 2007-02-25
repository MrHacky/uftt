#ifndef MAIN_H
  #define MAIN_H

  #include "stdafx.h"

  int send_msg(const std::string& msg, int port);
  int recv_msg(std::string &msg, int port, std::string* from);
  SOCKET UsePort(int p);
  SOCKET UseInterface(int i);
  SOCKET UseUDP(bool b);
#endif
