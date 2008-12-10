#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <boost/asio.hpp>
#include "net-asio/asio_file_stream.h"
#include "AutoUpdate.h"

/*** evil globals here, TODO: remove them all! ***/
extern services::diskio_service* gdiskio;
extern AutoUpdater updateProvider;
extern std::string thebuildstring;
extern boost::rand48 rng;

#endif//GLOBALS_H
