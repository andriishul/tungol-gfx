#pragma once
#include <cstdint>

namespace tungol::asset 
{
	constexpr uint32_t BC_MAGIC = 0x4342;

	struct BcTextureHeader 
	{
		uint32_t magic;

		uint32_t width;
		uint32_t height;

		uint32_t format;

		uint32_t blockBytes;
		uint32_t mipLevels;
	};
}