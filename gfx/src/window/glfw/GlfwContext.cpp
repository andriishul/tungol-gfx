#include "GlfwContext.hpp"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

// getc
#include <cstdio>
#include <stdexcept>

#include <event/EventBus.hpp>
#include <event/WinErrorEventArgs.hpp>
#include <event/MouseMovedEventArgs.hpp>

namespace tungol {


    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        //event::EventBus::instance().fire(
        //    event::EventType::WindowResize,
        //    event::WindowResizeEventArgs(width, height) // Assuming this struct exists
        //);
    }

    static void windowCloseCallback(GLFWwindow* window)
    {
        //event::EventBus::instance().fire(
        //event::EventType::WindowClose,
        //event::EventArgs() 
        //);
    }

    static void windowFocusCallback(GLFWwindow* window, int focused)
    {
       // event::EventType type = focused ? event::EventType::WindowFocus : event::EventType::WindowLostFocus;
       // event::EventBus::instance().fire(type, event::EventArgs());
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            /*event::EventBus::instance().fire(event::EventType::KeyPressed,event::KeyEventArgs(key, mods));*/
        }
        else if (action == GLFW_RELEASE)
        {
           //event::EventBus::instance().fire(event::EventType::KeyReleased,event::KeyEventArgs(key, mods));
        }
    }

    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
    {
        event::EventBus::instance().fire(event::EventType::MouseMoved, event::MouseMovedEventArgs((float) xpos, (float) ypos));
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            /*event::EventBus::instance().fire(event::EventType::MouseButtonPressed,event::MouseButtonEventArgs(button, mods));*/
        }
        else if (action == GLFW_RELEASE)
        {
            //event::EventBus::instance().fire(event::EventType::MouseButtonReleased,event::MouseButtonEventArgs(button, mods));
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
       //event::EventBus::instance().fire(event::EventType::MouseScrolled,event::MouseScrolledEventArgs((float)xoffset, (float)yoffset));
    }



	void GlfwContext::init()
	{
		glfwSetErrorCallback([](int error_code, const char* description)
		{
			event::EventBus::instance().fire(event::EventType::WindowError, event::WinErrorEventArgs(description));
		});
				
		if (!glfwInit())
		{
			throw std::runtime_error("Failed to initialize GLFW!");
		}

		if (!glfwVulkanSupported())
		{
			throw std::runtime_error("Vulkan loader and any minimally functional ICD have not been found. ");
		}

		glfwInitVulkanLoader((PFN_vkGetInstanceProcAddr)glfwGetProcAddress);
	}

	void GlfwContext::setEvents(GlfwWindow& window)
	{
		GLFWwindow* glfwWindow = reinterpret_cast<GLFWwindow*>(window.hostHandle());

        glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);
        glfwSetWindowCloseCallback(glfwWindow, windowCloseCallback);
        glfwSetWindowFocusCallback(glfwWindow, windowFocusCallback);
        glfwSetKeyCallback(glfwWindow, keyCallback);
        glfwSetCursorPosCallback(glfwWindow, cursorPositionCallback);
        glfwSetMouseButtonCallback(glfwWindow, mouseButtonCallback);
        glfwSetScrollCallback(glfwWindow, scrollCallback);
	}

	int GlfwContext::close()
	{
		glfwTerminate();

		return getchar();
	}
}
