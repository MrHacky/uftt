#include "SHA1.h"

#include <assert.h>

#include <iostream>
#include <boost/foreach.hpp>
#include "boost/filesystem/fstream.hpp"

using namespace std;

uint8 SHA1C::operator[](int i)
{
	assert(i >= 0 && i < 20);
	return data[i];
}

bool SHA1C::operator<(const SHA1C& o) const
{
	for (int i = 0; i < 20; ++i)
		if (data[i] < o.data[i])
			return true;
		else if (data[i] > o.data[i])
			return false;
	return false;
}

bool SHA1C::operator==(const SHA1C& o) const
{
	for (int i = 0; i < 20; ++i)
		if (data[i] != o.data[i])
			return false;
	return true;
}

/*
void SHA1::serialize(UFTT_packet* packet) const
{
	BOOST_FOREACH(const uint8& val, data) packet->serialize(val);
}

void SHA1::deserialize(UFTT_packet* packet)
{
	BOOST_FOREACH(uint8& val, data) packet->deserialize(val);
}
*/

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

void SHA1Hasher::Update(const SHA1C& data)
{
	Update(data.data, 20);
}

void SHA1Hasher::Update(const fs::path& data)
{
	fs::ifstream fstr;
	fstr.open(data, ios::binary);
	char buf[1024];
	while (fstr.read(buf, 1024))
		Update(buf, fstr.gcount());
	Update(buf, fstr.gcount());
}

SHA1C SHA1Hasher::GetResultSafe() const
{
	SHA1C res;
	GetResultSafe(res);
	return res;
}

void SHA1Hasher::GetResultSafe(SHA1C& hash) const
{
	SHA_CTX res = state;
	SHA1_Final(hash.data, &res);
}

SHA1C SHA1Hasher::GetResult()
{
	SHA1C res;
	GetResult(res);
	return res;
}

void SHA1Hasher::GetResult(SHA1C& hash)
{
	SHA1_Final(hash.data, &state);
}
