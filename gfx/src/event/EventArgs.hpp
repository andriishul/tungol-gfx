#pragma once
#include <string>
#define EVENT_TYPE(TYPE)	virtual const EventType type() const \
							{ return EventType::TYPE; } \
							virtual const std::string type_name() const { return #TYPE; } \
							
						  
namespace tungol::event
{
	enum class EventType 
	{
		None,

		Graphics,

		WindowError,
		WindowClose,
		WindowResize,
		WindowFocus,
		WindowLostFocus,

		KeyPressed,
		KeyReleased,
		MouseMoved,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseScrolled
	};

	class EventArgs 
	{
	public:
		virtual const std::string to_string() const = 0;
		
		EVENT_TYPE(None)
	};
}
