#pragma once
#include <functional>

namespace tungol::vhl 
{
	class Trackable 
	{
	protected:
		friend class HazardTracker;
		Trackable() = default;
		std::function<void(Trackable*)> dispose;
	};

}
