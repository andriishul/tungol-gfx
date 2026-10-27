#pragma once
#include "RenderKernel.hpp"
#include "RenderScene.hpp"

#include <core/ViewData.hpp>

namespace tungol::runtime
{
	class TUNGOL_EXPORT Renderer 
	{
	public:
		Renderer(RenderKernel& kernel);
		~Renderer();

		void begin();
		void draw(const RenderScene& renderScene, const core::ViewData& view); 
		void end();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
