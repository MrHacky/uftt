#include "stdafx.h"
#include "packet.h"
#include "main.h"

using namespace std;






void serialize(uint8** buf, uint8 val) {
	*((*buf)++) = val;
}

void serialize(uint8** buf, uint16 val) {
	*((*buf)++) = (val >>  0) & 0xFF;
	*((*buf)++) = (val >>  8) & 0xFF;
}

void serialize(uint8** buf, uint32 val) {
	*((*buf)++) = (val >>  0) & 0xFF;
	*((*buf)++) = (val >>  8) & 0xFF;
	*((*buf)++) = (val >> 16) & 0xFF;
	*((*buf)++) = (val >> 24) & 0xFF;
}

void serialize(uint8** buf, uint64 val) {
	*((*buf)++) = (val >>  0) & 0xFF;
	*((*buf)++) = (val >>  8) & 0xFF;
	*((*buf)++) = (val >> 16) & 0xFF;
	*((*buf)++) = (val >> 24) & 0xFF;
	*((*buf)++) = (val >> 32) & 0xFF;
	*((*buf)++) = (val >> 40) & 0xFF;
	*((*buf)++) = (val >> 48) & 0xFF;
	*((*buf)++) = (val >> 56) & 0xFF;
}

void serialize(uint8** buf, const string& str) {
	serialize(buf, (uint32)str.size());
	for (int i = 0; i < str.size(); ++i)
		serialize(buf, (uint8)str[i]);
}

void deserialize(uint8** buf, uint8 &val) {
	val = *((*buf)++);
}

void deserialize(uint8** buf, uint16 &val) {
	val = 0;
	val |= *((*buf)++) <<  0;
	val |= *((*buf)++) <<  8;
}

void deserialize(uint8** buf, uint32 &val) {
	val = 0;
	val |= *((*buf)++) <<  0;
	val |= *((*buf)++) <<  8;
	val |= *((*buf)++) << 16;
	val |= *((*buf)++) << 24;
}

void deserialize(uint8** buf, uint64 &val) {
	val = 0;
	val |= (uint64)*((*buf)++) <<  0;
	val |= (uint64)*((*buf)++) <<  8;
	val |= (uint64)*((*buf)++) << 16;
	val |= (uint64)*((*buf)++) << 24;
	val |= (uint64)*((*buf)++) << 32;
	val |= (uint64)*((*buf)++) << 40;
	val |= (uint64)*((*buf)++) << 48;
	val |= (uint64)*((*buf)++) << 56;
}

void deserialize(uint8** buf, string &str) {
	uint32 size;
	deserialize(buf, size);
	str.clear();
	for (int i = 0; i < size; ++i) {
		uint8 c;
		deserialize(buf, c);
		str.push_back(c);
	}
}

int send_packet(UFTT_packet* p, SOCKET tsock)
{
	sockaddr target_addr;
	sockaddr_ipx* ipx_addr =( sockaddr_ipx* )&target_addr;
	sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;

	ipx_addr->sipx_family = AF_IPX;
	for ( int i=0; i<4; ++i )
		(( uint8* )&ipx_addr->sa_netnum )[i] = 0;
	for ( int i=0; i<6; ++i )
		ipx_addr->sa_nodenum[i] = 0xFF;

	ipx_addr->sa_socket = htons( SERVER_PORT );

	// UDP:
	if ( udp_hax ) {
		udp_addr->sin_family = AF_INET;
		udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
		udp_addr->sin_port = htons( SERVER_PORT );
	}

// cout << "Got Address: " << addr2str(&target_addr) << "\n";
	uint8 mbuf[1500];
	uint8* index = mbuf;

	serialize(&index, p->type);

  switch (p->type) {
  	case PT_RESTART_SERVER : break;
  	case PT_QUERY_SERVERS  : break;
  	case PT_QUERY_SHARELIST:
			memcpy(&target_addr, ((UFTT_packet_shareinfo*)p)->serverinfo->address, sizeof(sockaddr));
  		break;
  	case PT_QUERY_SHAREINFO: break;
  	case PT_REPLY_SERVERS  : break;
  	case PT_REPLY_SHARELIST:
			serialize(&index, ((reply_servers*)p)->num_shares);
			serialize(&index, ((reply_servers*)p)->share_num);
			serialize(&index, ((reply_servers*)p)->UID);
			serialize(&index, ((reply_servers*)p)->name);
  	break;
  	case PT_REPLY_SHAREINFO: break;
  	case PT_REQUEST_CHUNK  : break;
  	case PT_SEND_CHUNK     : break;
  }

	int retval;
// cout << "Broadcasting test Packet...";
	retval = sendto( tsock, mbuf, index - mbuf, 0, (const sockaddr*)&target_addr, sizeof( target_addr ) );

	if ( retval == SOCKET_ERROR ) {
//		cout << "Failed! :" << NetGetLastError() << "\n";
		return 0;
	};
// cout << "Success! :" << retval << "\n";
	return retval;
}
