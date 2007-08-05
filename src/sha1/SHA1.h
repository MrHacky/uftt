#ifndef SHA1_H
#define SHA1_H

extern "C" {
#include "sha1_impl.h"
}

#include "../Types.h"

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

struct SHA1 {
	uint8 data[20];
	uint8 operator[](int i);
	bool operator<(const SHA1& o);
};

class SHA1Hasher {
	private:
		SHA_CTX state;
	public:
		SHA1Hasher();

		void Update(const void *dataIn, int len);
		void Update(const std::string& data);
		void Update(const fs::path& data);
		void Update(const SHA1& data);

		SHA1 GetResultSafe() const;
		void GetResultSafe(SHA1& hash) const;

		SHA1 GetResult();
		void GetResult(SHA1& hash);
};

#endif
