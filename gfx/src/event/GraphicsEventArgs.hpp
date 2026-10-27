#pragma once
#include "EventArgs.hpp"
#include <sstream>

namespace tungol::event
{
	enum class GraphicsAPI
	{
		None,
		Vulkan
	};

	enum class MessageType 
	{
		None,
		Verbose,
		Info,
		Warning,
		Error
	};


	inline std::ostream& operator<<(std::ostream& os, const GraphicsAPI& api)
	{
		switch (api)
		{
			case GraphicsAPI::Vulkan: return os << "Vulkan";
			default: return os << "None";
		}
	}


	inline std::ostream& operator<<(std::ostream& os, const MessageType& type)
	{
		switch (type)
		{
			case MessageType::Verbose: return os << "Verbose";
			case MessageType::Info: return os << "Info";
			case MessageType::Warning: return os << "Warning";
			case MessageType::Error: return os << "Error";
			default: return os << "None";
		}
	}

	class GraphicsEventArgs : public EventArgs
	{
	public:
		GraphicsEventArgs(GraphicsAPI api, const MessageType type, const std::string& description)
		{
			std::ostringstream ss;
			ss << "[" << api << "]" << "[" << type << "]: " << description;

			m_message = ss.str();
		}

		virtual const std::string to_string() const override { return m_message; }

		EVENT_TYPE(Graphics)
	private:
		std::string m_message;

	};
}
