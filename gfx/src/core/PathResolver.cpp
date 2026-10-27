#include "PathResolver.hpp"

namespace tungol::core
{
#if defined(TUNGOL_SHADER_DIR)
	std::string PathResolver::s_ShaderRoot = TUNGOL_SHADER_DIR;
#else
	#error "TUNGOL_SHADER_DIR is not defined! Did you forget to set target_compile_definitions in CMake?"
#endif

#if defined(TUNGOL_ASSET_DIR)
	std::string PathResolver::s_AssetRoot = TUNGOL_ASSET_DIR;
#else
	#error "TUNGOL_ASSET_DIR is not defined! Did you forget to set target_compile_definitions in CMake?"
#endif
	std::string PathResolver::GetShaderPath(const std::string& path)
	{
		return s_ShaderRoot + "/" + path;
	}
	std::string PathResolver::GetAssetPath(const std::string& path)
	{
		return s_AssetRoot + "/" + path;
	}
}