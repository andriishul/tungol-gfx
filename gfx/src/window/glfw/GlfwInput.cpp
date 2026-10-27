#include "GlfwInput.hpp"
#include <GLFW/glfw3.h>


namespace tungol {

	GlfwInput::GlfwInput(GlfwWindow& window) : m_window(window)
	{
		
	}
	bool GlfwInput::pressed(Key key) const
	{
		return glfwGetKey(reinterpret_cast<GLFWwindow*>(m_window.hostHandle()), static_cast<int>(key)) == GLFW_PRESS;
	}
	bool GlfwInput::released(Key key) const
	{
		return glfwGetKey(reinterpret_cast<GLFWwindow*>(m_window.hostHandle()), static_cast<int>(key)) == GLFW_RELEASE;
	}

	bool GlfwInput::repeated(Key key) const
	{
		return glfwGetKey(reinterpret_cast<GLFWwindow*>(m_window.hostHandle()), static_cast<int>(key)) == GLFW_REPEAT;
	}
}