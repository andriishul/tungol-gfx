#pragma once
#include <vhl/core/Device.hpp>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT Semaphore 
	{
	public:
		Semaphore(const Device& device);
		~Semaphore();

		SemaphoreHandle handle() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
