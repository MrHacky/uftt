#ifndef JOB_REQUEST_H
#define JOB_REQUEST_H

#include "Types.h"

#include <string>
#include <vector>

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
		JobRequest(uint8 type) : jobtype(type), mtime(0) {};
	public:
		uint8 type() const { return jobtype; };

		int32 mtime;
};
typedef boost::shared_ptr<JobRequest> JobRequestRef;

class JobRequestTreeData : public JobRequest {
	public:
		JobRequestTreeData() : JobRequest(JRT_TREEDATA), gotinfo(false) {};

		// request info
		SHA1 hash;

		// reply info
		struct child_info {
			SHA1 hash;
			std::string name;
			child_info(SHA1 h, std::string n) : hash(h), name(n) {};
		};
		std::vector<child_info> children;

		// progress info
		uint32 childcount;
		uint32 chunkcount;
		uint32 curchunk;
		bool gotinfo;
};
typedef boost::shared_ptr<JobRequestTreeData> JobRequestTreeDataRef;

class JobRequestBlobData : public JobRequest {
	public:
		JobRequestBlobData() : JobRequest(JRT_BLOBDATA), gotinfo(false) {};

		// request info
		SHA1 hash;
		fs::path fpath;

		// reply info

		// progress info
		uint64 fsize;
		uint32 chunkcount;
		uint16 offset;
		uint32 curchunk;
		bool gotinfo;
};
typedef boost::shared_ptr<JobRequestBlobData> JobRequestBlobDataRef;

class JobRequestQueryServers : public JobRequest {
	public:
		JobRequestQueryServers() : JobRequest(JRT_SERVERINFO) {};
};
typedef boost::shared_ptr<JobRequestQueryServers> JobRequestQueryServersRef;

#endif
