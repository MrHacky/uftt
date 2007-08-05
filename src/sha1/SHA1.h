#ifndef SHA1_H
#define SHA1_H

#include "sha1_impl.h"

#include "../Types.h"

struct SHA1 {
	uint8 data[20];
	uint8 operator[](int i);
};

class SHA1Hasher {
	private:
		SHA_CTX state;
	public:
		SHA1Hasher();

		void Update(const void *dataIn, int len);

		SHA1 GetResultSafe() const;
		void GetResultSafe(SHA1& hash) const;

		SHA1 GetResult();
		void GetResult(SHA1& hash);
};

#endif
