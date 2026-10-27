#pragma once
#include <core/TungolExport.hpp>

#include <string>
#include <cstdint>

namespace tungol
{
	struct FramebufferSize { uint32_t width = 0, height = 0; };

	class TUNGOL_EXPORT WindowBase
	{
	public:
		virtual ~WindowBase() = default;

		virtual void pollEvents() = 0;

		enum class Backend {
			GLFW,
			SDL,
			Win32
		};

		virtual Backend backend() const = 0;

		virtual void* hostHandle() const = 0;
		
		virtual FramebufferSize pixelSize() const = 0;

		virtual void setTitle(const std::string& title) = 0;

		virtual void stop() = 0;

		virtual bool running() const = 0;

	};

	inline bool operator!=(const FramebufferSize& pixelSize1, const FramebufferSize& pixelSize2)
	{
		return pixelSize1.height != pixelSize2.height 
			|| pixelSize1.width != pixelSize2.width;
	}
}
