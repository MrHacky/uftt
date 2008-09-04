#include "Base64.h"

namespace {
	struct is_invalid_base64_char {
		bool operator()(uint8 chr) {
			if (chr >= 'A' && chr <= 'Z') return false;
			if (chr >= 'a' && chr <= 'z') return false;
			if (chr >= '0' && chr <= '9') return false;
			if (chr == '+') return false;
			if (chr == '/') return false;
			if (chr == '=') return false;
			return true;
		}
	};

	static uint8 b64_list[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	#define XX 100

	static int b64_index[256] = {
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
		52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,-1,XX,XX,
		XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
		15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
		XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
		41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
		XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	};

	#undef XX
}

namespace Base64 {
	std::vector<uint8> decode(const std::vector<uint8>& input)
	{
		if ((input.size() % 4) != 0)
			return std::vector<uint8>();

		std::vector<uint8> result;

		int endofs = 0;

		for (uint i = 0; i < input.size(); i += 4) {
			uint32 value = 0;

			for (uint j = 0; j < 4; ++j) {
				int cval = b64_index[input[i+j]];
				if (cval == 100)
					return std::vector<uint8>(); // invalid character in input
				else if (cval != -1 && endofs == 0) {
					value <<= 6;
					value |= cval;
				} else if (cval == -1) { // '=' character
					value <<= 6;
					++endofs;
				} else {
					return std::vector<uint8>(); // non-'=' character after '='
				}
			}

			for (int j = 2; j >= endofs; --j)
				result.push_back((uint8) ((value >> (8*j)) & 0xFF));
		}

		return result;
	}

	std::vector<uint8> encode(const std::vector<uint8>& input)
	{
		std::vector<uint8> result;

		int endofs = 0;

		uint i;
		for (i = 0; i < input.size()-3; i += 3) {
			uint32 value = 0;
			for (uint j = 0; j < 3; ++j) {
				value |= input[i+j];
				value <<= 8;
			}

			for (int j = 4; j > 0; --j)
				result.push_back(b64_list[(value >> (2 + 6*j)) & 0x3f]);
		}
		{
			uint32 value = 0;
			for (uint j = 0; j < 3; ++j) {
				if (i+j < input.size())
					value |= input[i+j];
				else
					++endofs;
				value <<= 8;
			}

			for (int j = 4; j > endofs; --j)
				result.push_back(b64_list[(value >> (2 + 6*j)) & 0x3f]);
			for (int j = 0; j < endofs; ++j)
				result.push_back('=');
		}

		return result;
	}

	void filter(std::vector<uint8>* data)
	{
		std::vector<uint8>::iterator rpos = std::remove_if(data->begin(), data->end(), is_invalid_base64_char());
		data->resize(rpos - data->begin());
	}
}
