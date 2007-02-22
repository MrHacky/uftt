#ifndef MAIN_H
  #define MAIN_H

  #include "stdhdr.h"
  #include "types.h"
  #include <string>
  using namespace std;

  int send_msg(const string& msg, int port);
  int recv_msg(string &msg, int port, string* from);
  int UsePort(int p);
  int UseInterface(int i);
  int UseUDP(bool b);
#endif
