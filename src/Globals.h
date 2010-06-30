#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <boost/asio.hpp>
#include <boost/random/linear_congruential.hpp>

#include "AutoUpdate.h"

/*** evil globals here, TODO: remove them all! ***/
extern AutoUpdater updateProvider;
extern boost::rand48 rng;

#endif//GLOBALS_H
