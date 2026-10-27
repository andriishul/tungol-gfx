#pragma once
#include <vhl/core/Device.hpp>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT Fence 
	{
	public:
		Fence(const Device& device);
		~Fence();

		bool ready() const;

		void wait();
		void reset();

		FenceHandle handle() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
