#pragma once
#include <core/TungolExport.hpp>
#include "GlfwWindow.hpp"

namespace tungol {

	class TUNGOL_EXPORT GlfwContext
	{
	public:
		static void init();

		static void setEvents(GlfwWindow& window);

		static int close();
	};

}
