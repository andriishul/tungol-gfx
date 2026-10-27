#pragma once
#include <window/Input.hpp>
#include "GlfwWindow.hpp"

namespace tungol 
{
	class TUNGOL_EXPORT GlfwInput : public Input
	{
	public:
		GlfwInput(GlfwWindow& window);

		bool pressed(Key key) const;
		bool released(Key key) const;
		bool repeated(Key key) const;
	private:
		GlfwWindow& m_window;
	};
}