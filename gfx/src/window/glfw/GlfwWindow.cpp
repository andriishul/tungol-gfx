#include "GlfwWindow.hpp"
#include <stdexcept>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

namespace tungol {

	GlfwWindow::GlfwWindow(const std::string& title,
		uint32_t width, uint32_t height) 
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_Handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
		if (!m_Handle) throw std::runtime_error("[GlfwWindow] Failed to create window");

		
	}

	GlfwWindow::~GlfwWindow()
	{
		if(m_Handle)
			glfwDestroyWindow(reinterpret_cast<GLFWwindow*>(m_Handle));
	}

	FramebufferSize GlfwWindow::pixelSize() const
	{
		int w = 0, h = 0;
		glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(m_Handle), &w, &h);
		return FramebufferSize{ static_cast<uint32_t>(w),  static_cast<uint32_t>(h) };
	}

	void GlfwWindow::setTitle(const std::string& title)
	{
		glfwSetWindowTitle(reinterpret_cast<GLFWwindow*>(m_Handle), title.c_str());
	}

	void GlfwWindow::stop()
	{
		glfwSetWindowShouldClose(reinterpret_cast<GLFWwindow*>(m_Handle), GLFW_TRUE);
	}

	bool GlfwWindow::running() const
	{
		return !glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(m_Handle));
	}

	void GlfwWindow::pollEvents()
	{
		glfwPollEvents();
	}

}
