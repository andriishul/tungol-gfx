#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

namespace tungol::runtime
{
	class TUNGOL_EXPORT ShadowResources 
	{
	public:
		ShadowResources(RenderKernel& kernel);
		~ShadowResources();

		vhl::Image& image() const;
		vhl::ImageView& view() const;
		const vhl::DescriptorSetHandle& set() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
