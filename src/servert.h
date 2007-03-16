#ifndef SERVERT_H
#define SERVERT_H

/*FIXME: These #defines need to be user configurable at runtime*/
//-------------------------------------------------------------//
#define SERVER_PORT 55555
#define RECV_BUFFER_SIZE 1400
extern SOCKET ServerSock;
//-------------------------------------------------------------//

int WINAPI ServerThread(bool * Restart);
#endif
