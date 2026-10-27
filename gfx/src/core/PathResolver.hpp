#pragma once
#include "TungolExport.hpp"

#include <string>

namespace tungol::core
{
	class TUNGOL_EXPORT PathResolver 
	{
	public:
		static std::string GetShaderPath(const std::string& path);
		static std::string GetAssetPath(const std::string& path);
	private:
		static std::string s_ShaderRoot;
		static std::string s_AssetRoot;
	};
}