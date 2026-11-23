#pragma once
#include "RenderKernel.hpp"

#include <core/ViewData.hpp>

namespace tungol::runtime
{
	class TUNGOL_EXPORT Renderer 
	{
	public:
		Renderer(RenderKernel& kernel);
		~Renderer();

		void render(const core::ViewData& view, const core::Scene& scene);
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
