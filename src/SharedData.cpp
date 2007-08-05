#include "SharedData.h"

std::vector<ShareInfo> MyShares;
boost::mutex shares_mutex;

volatile bool terminating;

std::vector<JobRequestRef> JobQueue;
boost::mutex jobs_mutex;


