#pragma once
#include "Queue.hpp"
#include "Handles.hpp"

#include "Limits.hpp"
#include <string>


namespace tungol::vhl 
{
	class TUNGOL_EXPORT Device 
	{
	public:
		Device(const struct DeviceDesc& desc);
		~Device();

		void wait();

		const std::string& name() const;

		QueueFamilyList& queueFamilyList() const;
		
		DeviceLimits limits() const;
		
		DeviceHandle handle() const;
		AdapterHandle adapterHandle() const;
	private:
		struct Impl;
 		std::unique_ptr<Impl> m_impl;
	};
}
