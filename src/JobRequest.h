#ifndef JOB_REQUEST_H
#define JOB_REQUEST_H

#include "Types.h"

#include <string>
#include <vector>
#include <queue>

#include <time.h>

#include <boost/shared_ptr.hpp>

#include "sha1/SHA1.h"

enum {
	JRT_NONE = 0,
	JRT_TREEDATA,
	JRT_BLOBDATA,
	JRT_SERVERINFO,
};

class JobRequest {
	private:
		const uint8 jobtype;
		JobRequest();
		JobRequest(const JobRequest& o);
	protected:
		JobRequest(uint8 type) : jobtype(type), mtime(0), start_time(clock()) {};
	public:
		uint8 type() const { return jobtype; };

		int32 mtime;
		clock_t start_time;
};
typedef boost::shared_ptr<JobRequest> JobRequestRef;

class JobRequestTreeData : public JobRequest {
	public:
		JobRequestTreeData() : JobRequest(JRT_TREEDATA), gotinfo(false) {};

		// request info
		SHA1C hash;

		// reply info
		struct child_info {
			SHA1C hash;
			std::string name;
			child_info(SHA1C h, std::string n) : hash(h), name(n) {};
		};
		std::vector<child_info> children;

		// progress info
		uint32 childcount;
		uint32 chunkcount;
		uint32 curchunk;
		bool gotinfo;
};
typedef boost::shared_ptr<JobRequestTreeData> JobRequestTreeDataRef;

#define MAX_BUFFER_SIZE 256
#define MAX_CHUNK_SIZE 1024 // haxxy...

class JobRequestBlobData : public JobRequest {
	public:
		uint8 buffer[MAX_BUFFER_SIZE][MAX_CHUNK_SIZE];
		bool usebuf[MAX_BUFFER_SIZE];
	public:
		JobRequestBlobData();
		~JobRequestBlobData();

		// request info
		SHA1C hash;
		fs::path fpath;

		// reply info

		// progress info
		uint64 fsize;
		uint32 chunkcount;
		uint16 offset;
		uint32 curchunk;
		bool gotinfo;

		void handleChunk(uint64 offset, uint32 len, uint8* buf);
		void handleChunk(uint32 chunknum, uint32 chunksize, uint8* buf);

		bool timeout(uint32& reqchunk, uint8& reqcnt);
};
typedef boost::shared_ptr<JobRequestBlobData> JobRequestBlobDataRef;

class JobRequestQueryServers : public JobRequest {
	public:
		JobRequestQueryServers() : JobRequest(JRT_SERVERINFO) {};
};
typedef boost::shared_ptr<JobRequestQueryServers> JobRequestQueryServersRef;

#endif
