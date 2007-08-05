#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <vector>

#include <boost/thread/mutex.hpp>

#include "files/FileInfo.h"

extern std::vector<ShareInfo> MyShares;
extern boost::mutex shares_mutex;

extern volatile bool terminating;

struct JobRequest {
	uint8 type;
	SHA1 hash;
	fs::path path;
};

extern std::vector<JobRequest> JobQueue;
extern boost::mutex jobs_mutex;
#endif
