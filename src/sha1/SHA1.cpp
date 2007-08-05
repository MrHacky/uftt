#include "SHA1.h"

#include <assert.h>

uint8 SHA1::operator[](int i)
{
	assert(i >= 0 && i < 20);
	return data[i];
}

SHA1Hasher::SHA1Hasher()
{
	SHA1_Init(&state);
}

void SHA1Hasher::Update(const void *dataIn, int len)
{
	SHA1_Update(&state, dataIn, len);
}

SHA1 SHA1Hasher::GetResultSafe() const
{
	SHA1 res;
	GetResultSafe(res);
	return res;
}

void SHA1Hasher::GetResultSafe(SHA1& hash) const
{
	SHA_CTX res = state;
	SHA1_Final(hash.data, &res);
}

SHA1 SHA1Hasher::GetResult()
{
	SHA1 res;
	GetResult(res);
	return res;
}

void SHA1Hasher::GetResult(SHA1& hash)
{
	SHA1_Final(hash.data, &state);
}
