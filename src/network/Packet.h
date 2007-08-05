#ifndef PACKET_H
#define PACKET_H

#include "../Types.h"

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>

enum packet_types {
	PT_QUERY_SERVERS=0,
	PT_QUERY_SHARELIST,
	PT_REPLY_SERVERS,
	PT_REPLY_SHARELIST,
	PT_REQUEST_CHUNK,
	PT_SEND_CHUNK,
	PT_RESTART_SERVER,
};


//FIXME: Needs to be configurable @runtime
#define PACKET_SIZE 1400


class UFTT_packet {
	public:
	//private:
		int curpos;
		uint8 data[PACKET_SIZE];
	//protected:
		template <typename T>
			typename boost::enable_if<boost::is_unsigned<T> >::type
			serialize(const T& var)
		{
			for (int i = 0; i < sizeof(T); ++i)
				data[curpos++] = (var >> (i*8)) & 0xFF;
		}
		template <typename T>
			typename boost::enable_if<boost::is_unsigned<T> >::type
			deserialize(T& var)
		{
			var = 0;
			for (int i = 0; i < sizeof(T); ++i)
				var |= data[curpos++] << (i*8);
		}
};

#endif
