#pragma once
#include "EventArgs.hpp"
#include <sstream>

namespace tungol::event
{
	class MouseMovedEventArgs : public EventArgs
	{
	public:
		MouseMovedEventArgs(float xPos, float yPos)
			: m_xPos(xPos), m_yPos(yPos)
		{
		}

		float xPos() const { return m_xPos; }
		float yPos() const { return m_yPos; }

		virtual const std::string to_string() const override
		{
			std::ostringstream ss;
			ss << "[Mouse moved]: x=" << m_xPos << " y=" << m_yPos;
			return ss.str();
		}

		EVENT_TYPE(MouseMoved)
	private:
		float m_xPos = 0, m_yPos = 0;
	};
}