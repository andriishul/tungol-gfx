#pragma once
#include "EventArgs.hpp"

#include <functional>
#include <unordered_map>

namespace tungol::event
{ 
	class TUNGOL_EXPORT EventBus
	{
	public:
		using event_callback_t = std::function<void(const EventArgs& args)>;

		EventBus() = delete;

		EventBus& sub(
			EventType type, 
			const event_callback_t& callback
		)
		{
			m_callbacks.emplace(type, callback);
			return *this;
		}

		EventBus& unsub(EventType type)
		{
			m_callbacks.erase(type);
			return *this;
		}

		
		void fire(EventType type, const EventArgs& args)
		{
			if (auto it = m_callbacks.find(type); it != m_callbacks.cend()) 
			{
				it->second(args);
				return;
			}
		}
		static inline EventBus& instance() 
		{
			static EventBus bus{true};

			return bus;
		}

	private:
		explicit EventBus(bool) {}
		std::unordered_map<EventType, event_callback_t> m_callbacks;
	};

	inline void subscribe_graphics_default_logger()
	{
		EventBus::instance().sub(EventType::Graphics, [](const tungol::event::EventArgs& args)
		{
			printf("%s\n", args.to_string().c_str());
		});
	}
}


