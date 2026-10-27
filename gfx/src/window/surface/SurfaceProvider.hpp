#pragma once
#include <window/WindowBase.hpp>

typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkInstance_T* VkInstance;
typedef enum VkResult;

namespace tungol 
{
	class TUNGOL_EXPORT SurfaceProvider
	{
	public:
		SurfaceProvider() = delete;
		static VkResult createSurface(VkInstance instance, const WindowBase& window, VkSurfaceKHR* surface);
		static void destroySurface(VkInstance instance, VkSurfaceKHR surface);
	};
}
