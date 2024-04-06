#include "Base64.hpp"
#include <iostream>

namespace Engine
{
	static const std::string base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";


	static inline bool is_base64(uint8_t c)
	{
		return (isalnum(c) || (c == '+') || (c == '/'));
	}

	constexpr std::string Base64::Encode(const uint8_t* data, size_t length)
	{
		std::string ret;
		uint32_t i = 0;
		uint32_t j = 0;
		uint8_t char_array_3[3];
		uint8_t char_array_4[4];

		while (--length) {
			char_array_3[i++] = *(data++);
			if (i == 3) {
				char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
				char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
				char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
				char_array_4[3] = char_array_3[2] & 0x3f;

				for (i = 0; i < 4; ++i)
					ret += base64_chars[char_array_4[i]];
				i = 0;
			}
		}

		if (i)
		{
			for (j = i; j < 3; ++j)
				char_array_3[j] = '\0';

			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (j = 0; j < i + 1; ++j)
				ret += base64_chars[char_array_4[j]];

			while (++i < 3)
				ret += '=';
		}

		return ret;
	}

	constexpr std::vector<uint8_t> Base64::Decode(const char* data, size_t length)
	{
		if (length == 0)
			length = strlen(data);

		uint32_t i = 0;
		uint32_t j = 0;
		uint32_t in_ = 0;
		uint8_t char_array_4[4], char_array_3[3];
		std::vector<uint8_t> ret;

		while (--length && (data[in_] != '=') && is_base64(data[in_]))
		{
			char_array_4[i++] = data[in_]; in_++;
			if (i == 4)
			{
				for (i = 0; i < 4; ++i)
					char_array_4[i] = base64_chars.find(char_array_4[i]);

				char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
				char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
				char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

				for (i = 0; i < 3; ++i)
					ret.push_back(char_array_3[i]);
				i = 0;
			}
		}

		if (i)
		{
			for (j = i; j < 4; ++j)
				char_array_4[j] = 0;

			for (j = 0; j < 4; ++j)
				char_array_4[j] = base64_chars.find(char_array_4[j]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (j = 0; j < i - 1; ++j)
				ret.push_back(char_array_3[j]);
		}

		return ret;
	}
}