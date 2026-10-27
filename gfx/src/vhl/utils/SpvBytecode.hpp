#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include <fstream>

namespace tungol::vhl::utils
{  
	static std::vector<uint32_t> LoadSpvBytecode(const std::string& filename) 
	{
		std::ifstream ifs;
		ifs.open(filename, std::ios::binary | std::ios::in | std::ios::ate);

		if (!ifs.is_open()) 
		{
			throw std::runtime_error("[LoadSpvBytecode] Failed to open spv file: " + filename);
		}

		uint32_t endPos = (uint32_t) ifs.tellg();
		ifs.seekg(0, std::ios::beg);

		if (endPos % sizeof(uint32_t)) throw std::runtime_error("[LoadSpvBytecode] Invalid spv file! Not divisible by 4.");

		uint32_t size = endPos / sizeof(uint32_t);

		std::vector<uint32_t> byteCode(size);

		char* cbyteCode = reinterpret_cast<char*>(byteCode.data());
		ifs.read(cbyteCode, endPos);

		return byteCode;
	}
}
