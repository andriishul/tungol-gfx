#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <memory>

namespace tungol::runtime 
{
	class TUNGOL_EXPORT ShadowMap 
	{
	public:
		ShadowMap(RenderKernel& kernel, const vhl::Format fmt, uint32_t resolution);
		~ShadowMap();

		vhl::Image& image();
		vhl::ImageView& view();
		const vhl::DescriptorSetHandle& set() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}