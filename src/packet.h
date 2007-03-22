#ifndef PACKET_H
#define PACKET_H

#include "stdafx.h"
#include "servert.h"
#include "sharelister.h"

enum packet_types {
	PT_QUERY_SERVERS=0,
	PT_QUERY_SHARELIST,
	PT_QUERY_SHAREINFO,
	PT_REPLY_SERVERS,
	PT_REPLY_SHARELIST,
	PT_REPLY_SHAREINFO,
	PT_REQUEST_CHUNK,
	PT_SEND_CHUNK,
	PT_RESTART_SERVER,
};


//FIXME: Needs to be configurable @runtime
#define PACKET_SIZE 1400


struct UFTT_packet {
	uint8 type;
	uint8 data[PACKET_SIZE];
};

struct reply_servers {
	uint8 type;

	uint32 num_shares;
	uint32 share_num;
	uint64 UID;
	std::string name;
};

struct UFTT_packet_shareinfo {
  uint8 type;
  ServerInfo* serverinfo;
};

int send_packet(UFTT_packet* p, SOCKET tsock);

void deserialize(uint8** buf, uint8 &val);
void deserialize(uint8** buf, uint16 &val);
void deserialize(uint8** buf, uint32 &val);
void deserialize(uint8** buf, uint64 &val);
void deserialize(uint8** buf, std::string &str);


#endif
