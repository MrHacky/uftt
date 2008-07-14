#ifndef TYPES_H
#define TYPES_H

#include <boost/cstdint.hpp>

typedef boost::uint8_t  uint8;
typedef boost::int8_t   int8;

typedef boost::uint16_t uint16;
typedef boost::int16_t  int16;

typedef boost::uint32_t uint32;
typedef boost::int32_t  int32;

typedef boost::uint64_t uint64;
typedef boost::int64_t  int64;

typedef unsigned int uint;

// TODO: put this somewhere else?
#include <vector>
#include <boost/shared_ptr.hpp>

typedef boost::shared_ptr<std::vector<uint8> > shared_vec;
#define GETBUF(x) \
	(	(x->empty()) \
	?	(boost::asio::buffer((uint8*)NULL, 0)) \
	:	(boost::asio::buffer(&((*x)[0]), x->size())) \
	)

#endif
