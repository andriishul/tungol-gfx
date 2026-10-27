#pragma once
#include "EventArgs.hpp"
#include <sstream>

namespace tungol::event 
{
	class WinErrorEventArgs : public EventArgs
	{
	public:
		WinErrorEventArgs(const std::string& message) 
			: m_message(message)
		{
		}
		virtual const std::string to_string() const override 
		{
			std::ostringstream ss;
			ss << "[Error description]: " << m_message;
			return ss.str();
		}

		EVENT_TYPE(WindowError)
	private:
		std::string m_message;
	};
}


