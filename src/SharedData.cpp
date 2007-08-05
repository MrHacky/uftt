#include "SharedData.h"

std::vector<ShareInfo> MyShares;
boost::mutex shares_mutex;

volatile bool terminating;


