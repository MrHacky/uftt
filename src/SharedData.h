#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <vector>

#include <boost/thread/mutex.hpp>

#include "files/FileInfo.h"

extern std::vector<ShareInfo> MyShares;
extern boost::mutex shares_mutex;

extern volatile bool terminating;
#endif
