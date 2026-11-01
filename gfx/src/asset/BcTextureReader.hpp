#pragma once
#include "BcFormat.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace tungol::asset 
{
	struct BcTexture 
	{
		BcTextureHeader header;

		std::vector<std::vector<uint8_t>> mips;
	};

	inline BcTexture load_bc_texture(const std::string& file)
	{
		std::ifstream fs(file, std::ios::binary);
		if (!fs) throw std::runtime_error("[load_texture] Failed to open: " + file);

		BcTexture tex{};
		fs.read(reinterpret_cast<char*>(&tex.header), sizeof tex.header);
		if (!fs) throw std::runtime_error("[load_texture] File too small: " + file);


		if (!tex.header.blockBytes || tex.header.mipLevels == 0 || tex.header.magic != BC_MAGIC)
			throw std::runtime_error("[load_texture] Bc texture format not recognized: " + file);

		tex.mips.resize(tex.header.mipLevels);

		for (uint32_t i = 0; i < tex.header.mipLevels; ++i)
		{
			const uint32_t w = std::max(1U, tex.header.width >> i);
			const uint32_t h = std::max(1U, tex.header.height >> i);
			const size_t size = size_t(std::max(1U, (w + 3) / 4)) * std::max(1U, (h + 3) / 4) * tex.header.blockBytes;

			tex.mips[i].resize(size);
			fs.read(reinterpret_cast<char*>(tex.mips[i].data()), size);
			if (!fs) throw std::runtime_error("[load_texture] Truncated file: " + file);
		}

		return tex;
	}
}