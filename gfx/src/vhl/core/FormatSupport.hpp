#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include "GraphicsCommon.hpp"

namespace tungol::vhl 
{
	struct ImageFormatDesc
	{
		uint32_t maxWidth;
		uint32_t maxHeight;
		
		uint32_t maxMipLevels;
		uint32_t maxArrayLayers;
		
		SampleCount samples;
	};

	class TUNGOL_EXPORT FormatSupport 
	{
	public:
		FormatSupport() = delete;

		static bool is_supported(const Device& device, Format format, ImageType imageType, ImageUsageFlags usage, ImageFormatDesc&);
	};
}
