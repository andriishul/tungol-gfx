#include "SurfaceProvider.hpp"
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>
// sdl
// ...

#include <stdexcept>

namespace tungol 
{
	
	VkResult SurfaceProvider::createSurface(VkInstance instance, const WindowBase& window, VkSurfaceKHR* surface)
	{
		if (window.backend() == WindowBase::Backend::GLFW) 
		{
			GLFWwindow* glfw_window = reinterpret_cast<GLFWwindow*>(window.hostHandle());

			return glfwCreateWindowSurface(instance, glfw_window, nullptr, surface);
		}

		throw std::runtime_error("[SurfaceProvider::createSurface] Surface for current window not implemented !");
	}
	void SurfaceProvider::destroySurface(VkInstance instance, VkSurfaceKHR surface)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}
}