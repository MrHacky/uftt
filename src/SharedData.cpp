#include "SharedData.h"

std::vector<ShareInfo> MyShares;
boost::mutex shares_mutex;

volatile bool terminating;

std::vector<JobRequest> JobQueue;
boost::mutex jobs_mutex;


