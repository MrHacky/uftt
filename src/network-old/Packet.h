#ifndef PACKET_H
#define PACKET_H

#include <string>

#include "../Types.h"

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>

enum packet_types {
	PT_QUERY_SERVERS=0,
	PT_QUERY_SHARELIST,
	PT_REPLY_SERVERS,
	PT_REPLY_SHARELIST,

	PT_QUERY_OBJECT_INFO,
	PT_QUERY_TREE_DATA,
	PT_QUERY_BLOB_DATA,

	PT_REPLY_BLOB_INFO,
	PT_REPLY_TREE_INFO,
	PT_REPLY_BLOB_DATA,
	PT_REPLY_TREE_DATA,

	PT_RESTART_SERVER,
};

//TODO: Needs to be configurable @runtime
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
				data[curpos++] = (uint8)((var >> (i*8)) & 0xFF);
		}

		template <typename T>
			typename boost::enable_if<boost::is_unsigned<T> >::type
			deserialize(T& var)
		{
			var = 0;
			for (int i = 0; i < sizeof(T); ++i)
				var |= data[curpos++] << (i*8);
		}

		template <typename T>
			typename boost::disable_if<boost::is_fundamental<T> >::type
			serialize(const T& var)
		{
			var.serialize(this);
		}

		template <typename T>
			typename boost::disable_if<boost::is_fundamental<T> >::type
			deserialize(T& var)
		{
			var.deserialize(this);
		}

		void serialize(const std::string& var) {
			uint32 size = var.size();
			serialize(size);
			for (uint i = 0; i < size; ++i)
				serialize<uint8>(var[i]);
		}
		void deserialize(std::string& var) {
			uint32 size;
			deserialize(size);
			var.resize(size);
			for (uint i = 0; i < size; ++i) {
				uint8 t;
				deserialize(t);
				var[i] = t;
			}
		}
};

#endif
