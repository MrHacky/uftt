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

	udpsock = CreateUDPSocket(SERVER_PORT, NULL);
	assert(udpsock != INVALID_SOCKET);
	
	sockaddr bc_addr;
	{
		sockaddr_in* bc_inaddr = ( sockaddr_in * )&bc_addr;

		bc_inaddr->sin_family = AF_INET;
		bc_inaddr->sin_addr.s_addr = INADDR_BROADCAST;
		bc_inaddr->sin_port = htons( SERVER_PORT );
	}

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
		while (FD_ISSET(udpsock, &readset)) {
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
					case PT_QUERY_OBJECT_INFO: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						FileInfoRef fi = findFI(hash);
						if (!fi) {
							cout << "hash not found!" << endl;
							break;
						}

						if (fi->attrs & FATTR_DIR) {
							uint32 nchunks = 1;
							{ // count chunks
								uint32 cursize = 0;
								uint32 lastsize;
								uint32 maxsize = PACKET_SIZE;
								maxsize -= 1;  // type field
								maxsize -= 20; // hash field
								maxsize -= 4;  // chunk num field
								maxsize -= 4;  // zero len field
								BOOST_FOREACH(const FileInfoRef& cfi, fi->files) {
									lastsize = cursize;
									cursize += 4;                // 4 bytes for string (name) length
									cursize += cfi->name.size(); // x bytes for string (name) data
									cursize += 20;               // 20 bytes for hash
	
									if (cursize > maxsize) {
										++nchunks;
										cursize -= lastsize;
									}
								}
							}
							spacket.curpos = 0;
							spacket.serialize<uint8>(PT_REPLY_TREE_INFO);
							BOOST_FOREACH(uint8& val, hash.data) spacket.serialize(val);
							spacket.serialize<uint32>(fi->files.size());
							spacket.serialize(nchunks);
						} else {
							spacket.curpos = 0;
							spacket.serialize<uint8>(PT_REPLY_BLOB_INFO);
							BOOST_FOREACH(uint8& val, hash.data) spacket.serialize(val);
							spacket.serialize(fi->size);
						}

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &bc_addr, sizeof(bc_addr) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					}
					case PT_REPLY_TREE_INFO: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data) rpacket.deserialize(val);
						
						JobRequestTreeDataRef job = TreeJobs[hash];
						if (!job) {
							cout << "dont care for packet" << endl;
							break;
						}
						uint32 numfiles, numchunks;
						rpacket.deserialize(numfiles);
						rpacket.deserialize(numchunks);

						job->childcount = numfiles;
						job->chunkcount = numchunks;
						job->curchunk = 0;
						job->children.clear();
						job->gotinfo = true;
						job->mtime = 0;
						break;
					}
					case PT_QUERY_TREE_DATA: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data) rpacket.deserialize(val);
						FileInfoRef fi = findFI(hash);
						if (!fi) {
							cout << "hash not found!" << endl;
							break;
						}
						uint32 chunknum;
						rpacket.deserialize(chunknum);
						uint32 nchunk = 0;
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_TREE_DATA);
						BOOST_FOREACH(uint8& val, hash.data) spacket.serialize(val);
						spacket.serialize(chunknum);
						{ // count chunks
							uint32 cursize = 0;
							uint32 lastsize;
							uint32 maxsize = PACKET_SIZE;
							maxsize -= 1;  // type field
							maxsize -= 20; // hash field
							maxsize -= 4;  // chunk num field
							maxsize -= 4;  // zero len field
							BOOST_FOREACH(const FileInfoRef& cfi, fi->files) {
								lastsize = cursize;
								cursize += 4;                // 4 bytes for string (name) length
								cursize += cfi->name.size(); // x bytes for string (name) data
								cursize += 20;               // 20 bytes for hash

								if (cursize > maxsize) {
									++nchunk;
									cursize -= lastsize;
								}
								
								if (nchunk == chunknum) {
									spacket.serialize(cfi->name);
									BOOST_FOREACH(uint8& val, cfi->hash.data) spacket.serialize(val);
								}
							}
						}
						spacket.serialize(string(""));
						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &bc_addr, sizeof(bc_addr) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					}
					case PT_REPLY_TREE_DATA: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data) rpacket.deserialize(val);
						JobRequestTreeDataRef job = TreeJobs[hash];
						if (!job) {
							cout << "dont care for packet" << endl;
							break;
						}
						uint32 chunknum;
						rpacket.deserialize(chunknum);
						if (job->gotinfo && job->curchunk == chunknum) {
							string str;
							rpacket.deserialize(str);
							while (str != "") {
								SHA1 chash;
								BOOST_FOREACH(uint8& val, chash.data) rpacket.deserialize(val);
								job->children.push_back(JobRequestTreeData::child_info(chash, str));
								rpacket.deserialize(str);
							}
							++job->curchunk;
							job->mtime = 0;
						}
						break;
					}
					case PT_REPLY_BLOB_INFO: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data) rpacket.deserialize(val);

						{
							JobRequestTreeDataRef job = TreeJobs[hash];
							if (job) {
								job->children.clear();
								job->curchunk = 0;
								job->chunkcount = 0;
								job->childcount = 0;
								job->mtime = 0;
								job->gotinfo = true;
								
								cout << "dont care for packet";// << endl;
								FileInfoRef fi = findFI(hash);
								if (fi) cout << ":" << fi->name;
								cout << endl;

								break;
							}
						}
						JobRequestBlobDataRef job = BlobJobs[hash];
						if (!job) {
							cout << "no job" << endl;
							break;
						}
						uint64 fsize;
						rpacket.deserialize(fsize);
						job->fsize = fsize;
						job->chunkcount = (fsize >> 10) + 1;  // x/1024
						job->offset = fsize & ((1<<10)-1);
						job->curchunk = 0;
						job->gotinfo = true;
						job->mtime = 0;
						break;
					}
					case PT_QUERY_BLOB_DATA: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						fs::path fp = findfpath(hash);
						if (fp=="") {
							cout << "hash not found!" << endl;
							break;
						}
						cout << "found path:" << fp << endl;

						uint32 chunknum;
						rpacket.deserialize(chunknum);
						fs::ifstream fstr;
						fstr.open(fp, ios::binary);
						fstr.seekg(chunknum << 10, ios_base::beg);
						uint8 buf[1024];

						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_REPLY_BLOB_DATA);

						BOOST_FOREACH(uint8 & val, hash.data)
							spacket.serialize(val);

						spacket.serialize<uint32>(chunknum);
						fstr.read((char*)buf, 1024);
						uint32 len = fstr.gcount();
						for (int i = 0; i < len; ++i)
							spacket.serialize(buf[i]);

						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &source_addr, sizeof( source_addr ) ) == SOCKET_ERROR)
							cout << "error sending packet: " << NetGetLastError() << endl;
						break;
					}
					case PT_REPLY_BLOB_DATA: {
						SHA1 hash;
						BOOST_FOREACH(uint8& val, hash.data)
							rpacket.deserialize(val);
						JobRequestBlobDataRef job = BlobJobs[hash];
						if (!job) {
							cout << "no job" << endl;
							break;
						}
						uint32 chunknum;
						rpacket.deserialize(chunknum);
						if (chunknum != job->curchunk) {
							cout << "no need for chunk" << endl;
							break;
						}
						uint32 len = 1024;
						if (chunknum == job->chunkcount-1)
							len = job->offset;
						uint8 buf[1024];
						for (uint i = 0; i < len; ++i)
							rpacket.deserialize(buf[i]);

						if (len != 0) {
							// TODO: find out why this is needed (it kills the file, but why?)
							fs::fstream fstr; // need fstream instead of ofstream if we want to append
	
							// TODO: hmpf. not what i wanted, but works.... find out why
							fs::path fp = job->fpath;
							fstr.open(fp, ios::out | ios::binary | ios::app);
							//fstr.seekp(filepos, ios_base::beg);

							fstr.write((char*)buf, len);
						}

						++job->curchunk;
						job->mtime = 0;
						break;
					}
					default: {
						cout << "packet type uknown" << endl;
					}
				}
			}
			tv.tv_sec = 0;
			tv.tv_usec = 0;

			FD_ZERO(&readset);
			FD_SET(udpsock, &readset);

			// poll for incoming stuff
			sel = select(udpsock+1, &readset, NULL, NULL, &tv);
			assert(sel >= 0);
		}

		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			for (; JobQueue.size() > 0; JobQueue.pop_back()) {
				const JobRequestRef& job = JobQueue.back();
				MyJobs.push_back(job);
			}
		}

		for (int i = MyJobs.size()-1; i >= 0; --i) {
			JobRequestRef basejob = MyJobs[i];
			if (--basejob->mtime < 0) {
				basejob->mtime = 25;
				switch (basejob->type()) {
					case JRT_SERVERINFO: {
						spacket.curpos = 0;
						spacket.serialize<uint8>(PT_QUERY_SERVERS);
	
						if (sendto(udpsock, spacket.data, spacket.curpos, 0, &bc_addr, sizeof( bc_addr ) ) == SOCKET_ERROR)
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
								cbNewTreeInfo(job);
								TreeJobs[job->hash].reset();
								MyJobs.erase(MyJobs.begin() + i);
							}
						}

						if (spacket.curpos > 0) {
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &bc_addr, sizeof( bc_addr ) ) == SOCKET_ERROR)
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
								BlobJobs[job->hash].reset();
								MyJobs.erase(MyJobs.begin() + i);
							}
						}

						if (spacket.curpos > 0) {
							if (sendto(udpsock, spacket.data, spacket.curpos, 0, &bc_addr, sizeof( bc_addr ) ) == SOCKET_ERROR)
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
