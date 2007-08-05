#include "NetworkThread.h"

#include <iostream>
#include <map>

#include <boost/foreach.hpp>
#include "boost/filesystem/fstream.hpp"
#include <boost/pointer_cast.hpp>

#include "../SharedData.h"
#include "CrossPlatform.h"
#include "Packet.h"

using namespace std;

#define SERVER_PORT 12345

// TODO: improve/move elsewhere
FileInfoRef findFI(const FileInfoRef fi, const SHA1 & hash)
{
	if (hash == fi->hash)
		return fi;
	FileInfoRef res;
	BOOST_FOREACH(const FileInfoRef & tfi, fi->files) {
		res = findFI(tfi, hash);
		if (res) return res;
	}
	return res;
}

FileInfoRef findFI(const SHA1 & hash)
{
	FileInfoRef res;
	boost::mutex::scoped_lock lock(shares_mutex);
	BOOST_FOREACH(const ShareInfo & si, MyShares) {
		res = findFI(si.root, hash);
		if (res) return res;
	}
	return res;
}

fs::path findfpath(const FileInfoRef fi, const SHA1 & hash)
{
	if (hash == fi->hash)
		return fi->name;
	fs::path res = "";
	BOOST_FOREACH(const FileInfoRef & tfi, fi->files) {
		res = findfpath(tfi, hash);
		if (res != "") return fs::path(fi->name) / res;
	}
	return res;
}

fs::path findfpath(const SHA1 & hash)
{
	fs::path res = "";
	boost::mutex::scoped_lock lock(shares_mutex);
	BOOST_FOREACH(const ShareInfo & si, MyShares) {
		res = findfpath(si.root, hash);
		fs::path tmp = si.path;
		tmp.remove_leaf();
		if (res != "") return tmp / res;
	}
	return res;
}

static string addr2str( sockaddr* addr ) {
	char buf[100];
	switch ( addr->sa_family ) {
		case AF_IPX: {
			sockaddr_ipx* ipx_addr = ( sockaddr_ipx* )addr;
			snprintf( buf, 100, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
								(( uint8* )( &ipx_addr->sa_netnum ) )[0],
								(( uint8* )( &ipx_addr->sa_netnum ) )[1],
								(( uint8* )( &ipx_addr->sa_netnum ) )[2],
								(( uint8* )( &ipx_addr->sa_netnum ) )[3],
								( uint8 )ipx_addr->sa_nodenum[0], ( uint8 )ipx_addr->sa_nodenum[1],
								( uint8 )ipx_addr->sa_nodenum[2], ( uint8 )ipx_addr->sa_nodenum[3],
								( uint8 )ipx_addr->sa_nodenum[4], ( uint8 )ipx_addr->sa_nodenum[5],
								ntohs( ipx_addr->sa_socket ) );
		}
		; break;
		case AF_INET: {
			sockaddr_in* ip_addr = ( sockaddr_in* )addr;
			snprintf( buf, 100, "%u.%u.%u.%u:%i",
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[0],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[1],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[2],
								(( uint8* )( &ip_addr->sin_addr.s_addr ) )[3],
								ntohs( ip_addr->sin_port ) );
		}
		; break;
		default:
			snprintf( buf, 100, "Unknown address family: %i", addr->sa_family);
	}
	return string( buf );
}

static SOCKET CreateUDPSocket( uint16 bindport, sockaddr_in* iface_addr) {
	sockaddr_in addr;
	SOCKET sock;
	int retval;

	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( sock == INVALID_SOCKET ) {
		cout << "UDP: Failed to create socket: " << NetGetLastError() << "\n";
		return INVALID_SOCKET;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( bindport );

	retval = bind( sock, ( sockaddr* )&addr, sizeof( addr ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "UDP: Bind to network failed:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	}

	// now enable broadcasting
	unsigned long bc = 1;
	retval = setsockopt( sock, SOL_SOCKET, SO_BROADCAST, ( char* )&bc, sizeof( bc ) );
	if ( retval == SOCKET_ERROR ) {
		cout << "UDP: Unable to enable broadcasting:" << NetGetLastError() << "\n";
		closesocket( sock );
		return INVALID_SOCKET;
	};

	return sock;
}

void NetworkThread::operator()()
{
	SOCKET udpsock;
	UFTT_packet rpacket;
	UFTT_packet spacket;
	sockaddr source_addr;
	vector<JobRequestRef> MyJobs;
	map<SHA1, JobRequestTreeDataRef> TreeJobs;
	map<SHA1, JobRequestBlobDataRef> BlobJobs;
	map<SHA1, FileInfoRef> inqueuemap;
	map<SHA1, fs::path> downpath;

	udpsock = CreateUDPSocket(SERVER_PORT, NULL);
	assert(udpsock != INVALID_SOCKET);

	// initialise networkerrno 97
	while (!terminating) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(udpsock, &readset);

		// poll for incoming stuff
		int sel = select(udpsock+1, &readset, NULL, NULL, &tv);
		//cout << "sel:" << sel << '\n';
		socklen_t len = sizeof(source_addr);
		assert(sel >= 0);
		if (FD_ISSET(udpsock, &readset)) {
			int msglen = recvfrom(udpsock, rpacket.data, 1400, 0, &source_addr, &len);
			if (msglen == SOCKET_ERROR) {
				fprintf(stderr, "Server: recvfrom() failed with error #%i\n", NetGetLastError());
			} else {
				rpacket.curpos = 0;
				uint8 ptype;
				rpacket.deserialize(ptype);
				cout << "got packet,\ttype:" << (int)ptype;
				cout << "\t size:" << msglen;
				cout << "\tfrom:" << addr2str(&source_addr);
				cout << endl;
				switch (ptype) {
					case PT_QUERY_SERVERS: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_SERVERS);

						sockaddr_in* udp_addr = ( sockaddr_in * )&source_addr;

						udp_addr->sin_family = AF_INET;

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					};
					case PT_REPLY_SERVERS: {
						cbAddServer(); // TODO: supply some arguments?
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_QUERY_SHARELIST);

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					};
					case PT_QUERY_SHARELIST: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_SHARELIST);

						{
							boost::mutex::scoped_lock lock(shares_mutex);
							spacket.serialize<uint32>(MyShares.size());
							BOOST_FOREACH(const ShareInfo & si, MyShares) {
								spacket.serialize(si.root->name);
								// TODO: serialize SHA1 object nicer
								BOOST_FOREACH(uint8 val, si.root->hash.data) spacket.serialize(val);
							}
						}
						assert(spacket.curpos < 1400);
						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					};
					case PT_REPLY_SHARELIST: {
						uint32 numshares;
						rpacket.deserialize(numshares);
						for (int i = 0; i < numshares; ++i) {
							string name;
							SHA1 hash;
							rpacket.deserialize(name);
							// TODO: serialize SHA1 object nicer
							BOOST_FOREACH(uint8& val, hash.data) rpacket.deserialize(val);

							cbAddShare(name, hash);
						}
						
						break;
					}
					case PT_QUERY_CHUNK: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						FileInfoRef fi = findFI(hash);
						if (!fi) {
							cout << "hash not found!" << endl;
							break;
						}

						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_INFO_CHUNK);
						BOOST_FOREACH(uint8& val, hash.data) spacket.serialize(val);
						//spacket.serialize(fi->name);

						uint32 curfile;
						rpacket.deserialize(curfile);

						while (curfile < fi->files.size() && (spacket.curpos + 20 + fi->files[curfile]->name.size() + 15) < 1400) {
							spacket.serialize(fi->files[curfile]->name);
							BOOST_FOREACH(const uint8& val, fi->files[curfile]->hash.data)
								spacket.serialize(val);
							++curfile;
						}
						spacket.serialize(string(""));
						if (curfile >= fi->files.size()) curfile = 0;
						spacket.serialize(curfile);

						assert(spacket.curpos < 1400);
						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					}
					case PT_INFO_CHUNK: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						FileInfoRef fi = inqueuemap[hash];
						if (!fi) {
							fi = FileInfoRef(new FileInfo());
							inqueuemap[hash] = fi;
							fi->hash = hash;
						}
						string str;
						rpacket.deserialize(str);
						while (str != "") {
							FileInfoRef sfi(new FileInfo());
							sfi->name = str;
							BOOST_FOREACH(uint8& val, sfi->hash.data)
								rpacket.deserialize(val);
							fi->files.push_back(sfi);
							rpacket.deserialize(str);
						}
						uint32 nextpos;
						rpacket.deserialize(nextpos);

						if (nextpos == 0) {
							inqueuemap[hash].reset();
							FileInfoRef* cfir = new FileInfoRef(fi);
							cbNewFileInfo((void*)cfir);
						} else {
							spacket.curpos = 0;
							spacket.serialize<uint8>(PT_QUERY_CHUNK);

							BOOST_FOREACH(uint8 & val, hash.data)
								spacket.serialize(val);

							spacket.serialize<uint32>(nextpos);
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
								cout << "error sending packet: " << NetGetLastError() << endl;
						}

						break;
					}
					case PT_REQUEST_CHUNK: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						fs::path fp = findfpath(hash);
						if (fp=="") {
							cout << "hash not found!" << endl;
							break;
						}
						cout << "found path:" << fp << endl;

						uint32 filepos;
						rpacket.deserialize(filepos);
						fs::ifstream fstr;
						fstr.open(fp, ios::binary);
						fstr.seekg(filepos, ios_base::beg);
						uint8 buf[1024];

						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_SEND_CHUNK);

						BOOST_FOREACH(uint8 & val, hash.data)
							spacket.serialize(val);

						spacket.serialize<uint32>(fstr.tellg());
						bool eof = fstr.read((char*)buf, 1024);
						uint32 len = fstr.gcount();
						spacket.serialize(len);
						for (int i = 0; i < len; ++i)
							spacket.serialize(buf[i]);

						if (!eof)
							spacket.serialize<uint32>(0);
						else
							spacket.serialize<uint32>(fstr.tellg());

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					}
					case PT_SEND_CHUNK: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						fs::path fp = downpath[hash];
						if (fp=="") {
							cout << "hash not found!" << endl;
							break;
						}
						uint32 filepos;
						uint32 len;
						rpacket.deserialize(filepos);
						rpacket.deserialize(len);
						if (len != 0) {
							// TODO: find out why this is needed (it kills the file, but why?)
							fs::fstream fstr; // need fstream instead of ofstream if we want to append
	
							// TODO: hmpf. not what i wanted, but works.... find out why
							fstr.open(fp, ios::out | ios::binary | ios::app);
							//fstr.seekp(filepos, ios_base::beg);
	
							uint8 buf[1024];
							for (uint i = 0; i < len; ++i)
								rpacket.deserialize(buf[i]);

							fstr.write((char*)buf, len);
						}

						rpacket.deserialize(filepos);

						if (filepos != 0) {
							spacket.curpos = 0;
							spacket.serialize<uint8>(PT_REQUEST_CHUNK);

							BOOST_FOREACH(uint8 & val, hash.data)
								spacket.serialize(val);

							spacket.serialize<uint32>(filepos);
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
								cout << "error sending packet: " << NetGetLastError() << endl;
						}
						break;
					}
					default: {
						cout << "packet type uknown" << endl;
					}
				}
			}
		}

		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			for (; JobQueue.size() > 0; JobQueue.pop_back()) {
				const JobRequestRef& job = JobQueue.back();
				MyJobs.push_back(job);
			}
		}

		for (int i = MyJobs.size(); i > 0; --i) {
			JobRequestRef basejob = MyJobs[i];
			if (++basejob->time > 10) {
				switch (basejob->type()) {
					case JRT_SERVERINFO: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_QUERY_SERVERS);
						
						sockaddr target_addr;
						sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;
						
						udp_addr->sin_family = AF_INET;
						udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
						udp_addr->sin_port = htons( SERVER_PORT );
	
						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &target_addr, sizeof( target_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						MyJobs.erase(MyJobs.begin() + i);
						break;
					}
					case JRT_TREEDATA: {
						JobRequestTreeDataRef job = boost::static_pointer_cast<JobRequestTreeData>(basejob);
						TreeJobs[job->hash] = job;
						spacket.curpos = 0;
						if (!job->gotinfo) {
							spacket.serialize<uint8>(PT_QUERY_OBJECT_INFO);
	
							BOOST_FOREACH(uint8 & val, job->hash.data)
								spacket.serialize(val);
						} else {
							if (job->curchunk < job->chunkcount) {
								spacket.serialize<uint8>(PT_QUERY_TREE_DATA);
		
								BOOST_FOREACH(uint8 & val, job->hash.data)
									spacket.serialize(val);

								spacket.serialize(job->curchunk);
							} else {
								// TODO: callback
								MyJobs.erase(MyJobs.begin() + i);
							}
						}

						if (spacket.curpos > 0) {
							sockaddr target_addr;
							sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;
		
							udp_addr->sin_family = AF_INET;
							udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
							udp_addr->sin_port = htons( SERVER_PORT );
		
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &target_addr, sizeof( target_addr ) ) == SOCKET_ERROR)
								cout << "error sending packet: " << NetGetLastError() << endl;
						}
						break;
					}
					case JRT_BLOBDATA: {
						JobRequestBlobDataRef job = boost::static_pointer_cast<JobRequestBlobData>(basejob);;
						BlobJobs[job->hash] = job;
						spacket.curpos = 0;
						if (!job->gotinfo) {
							spacket.serialize<uint8>(PT_QUERY_OBJECT_INFO);
	
							BOOST_FOREACH(uint8 & val, job->hash.data)
								spacket.serialize(val);
						} else {
							if (job->curchunk < job->chunkcount) {
								spacket.serialize<uint8>(PT_QUERY_BLOB_DATA);
		
								BOOST_FOREACH(uint8 & val, job->hash.data)
									spacket.serialize(val);

								spacket.serialize(job->curchunk);
							} else {
								// TODO: callback
								MyJobs.erase(MyJobs.begin() + i);
							}
						}

						if (spacket.curpos > 0) {
							sockaddr target_addr;
							sockaddr_in* udp_addr = ( sockaddr_in * )&target_addr;
		
							udp_addr->sin_family = AF_INET;
							udp_addr->sin_addr.s_addr = INADDR_BROADCAST;
							udp_addr->sin_port = htons( SERVER_PORT );
		
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &target_addr, sizeof( target_addr ) ) == SOCKET_ERROR)
								cout << "error sending packet: " << NetGetLastError() << endl;
						}
						break;
					}
					default: {
						cout << "unknown job type: " << (int)basejob->type() << endl;
					}
				}
			}
		}

	}
}
