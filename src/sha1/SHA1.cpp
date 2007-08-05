#include "SHA1.h"

#include <assert.h>

#include <iostream>
#include "boost/filesystem/fstream.hpp"

using namespace std;

uint8 SHA1::operator[](int i)
{
	assert(i >= 0 && i < 20);
	return data[i];
}

bool SHA1::operator<(const SHA1& o) const
{
	for (int i = 0; i < 20; ++i)
		if (data[i] < o.data[i])
			return true;
	return false;
}

SHA1Hasher::SHA1Hasher()
{
	SHA1_Init(&state);
}

void SHA1Hasher::Update(const void *dataIn, int len)
{
	SHA1_Update(&state, dataIn, len);
}

void SHA1Hasher::Update(const std::string& data)
{
	Update(data.data(), data.size());
}

void SHA1Hasher::Update(const SHA1& data)
{
	Update(data.data, 20);
}

void SHA1Hasher::Update(const fs::path& data)
{
	fs::ifstream fstr;
	fstr.open(data, ios::binary);
	char buf[1024];
	int len;
	while (fstr.read(buf, 1024))
		Update(buf, fstr.gcount());
	Update(buf, fstr.gcount());
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
