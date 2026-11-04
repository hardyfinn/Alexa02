// Alexa02: kits. A kit holds every sound parameter of the six voices, the
// FX bus and the mod matrix, as the bytes the LXR firmware stores. The
// firmware's kit files (.snd) are an 8 byte name followed by these bytes,
// so kits from the hardware load as they are.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "ParamMeta.hpp"

namespace lxr {

struct Kit {
	char name[9] = "Init";
	uint8_t p[KIT_SIZE] = {};

	void setName(const std::string& n) {
		std::memset(name, 0, sizeof name);
		std::strncpy(name, n.c_str(), 8);
	}

	// load from the firmware file layout: 8 name bytes + parameters
	bool fromBytes(const uint8_t* data, size_t size) {
		if (size < 8) return false;
		char n[9] = {};
		std::memcpy(n, data, 8);
		for (int i = 0; i < 8; i++)
			if (n[i] && (n[i] < 32 || n[i] > 126)) n[i] = ' ';
		setName(n);
		for (int i = 0; i < KIT_SIZE; i++) {
			const size_t src = 8 + (size_t)i;
			p[i] = src < size ? data[src] : 0;
		}
		return true;
	}

	std::vector<uint8_t> toBytes() const {
		std::vector<uint8_t> out(8 + KIT_SIZE, 0);
		std::memcpy(out.data(), name, 8);
		std::memcpy(out.data() + 8, p, KIT_SIZE);
		return out;
	}

	std::string toHex() const {
		static const char* hex = "0123456789abcdef";
		std::string s;
		for (int i = 0; i < KIT_SIZE; i++) {
			s += hex[p[i] >> 4];
			s += hex[p[i] & 15];
		}
		return s;
	}

	void fromHex(const std::string& s) {
		auto nib = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return 0;
		};
		for (int i = 0; i < KIT_SIZE; i++) {
			const size_t j = (size_t)i * 2;
			p[i] = j + 1 < s.size() ? (uint8_t)(nib(s[j]) * 16 + nib(s[j + 1])) : 0;
		}
	}
};

} // namespace lxr
