#ifndef SHA1_H
#define SHA1_H

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
extern "C" {
#include "sha1_impl.h"
}
#endif

//extern "C" {

//}

#include "../Types.h"
//#include "../network/Packet.h"

#include "boost/filesystem/path.hpp"
namespace fs = boost::filesystem;

struct SHA1C {
	uint8 data[20];
	uint8 operator[](int i);
	bool operator<(const SHA1C& o) const;
	bool operator==(const SHA1C& o) const;

	//void serialize(UFTT_packet* packet) const;
	//void deserialize(UFTT_packet* packet);
};

class SHA1Hasher {
	private:
		SHA_CTX state;
	public:
		SHA1Hasher();

		void Update(const void *dataIn, int len);
		void Update(const std::string& data);
		void Update(const fs::path& data);
		void Update(const SHA1C& data);

		SHA1C GetResultSafe() const;
		void GetResultSafe(SHA1C& hash) const;

		SHA1C GetResult();
		void GetResult(SHA1C& hash);
};

#endif
