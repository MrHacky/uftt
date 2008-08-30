#ifndef BASE_64_H
#define BASE_64_H

#include "../types.h"
#include <vector>
#include <string>

namespace Base64 {
	std::vector<uint8> decode(const std::vector<uint8>& input);
	std::vector<uint8> encode(const std::vector<uint8>& input);
	void filter(std::vector<uint8>* data);
}

#endif//BASE_64_H
