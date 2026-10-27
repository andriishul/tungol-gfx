#pragma once
#include <window/WindowBase.hpp>

namespace tungol {

	class TUNGOL_EXPORT GlfwWindow : public WindowBase
	{
	public:
		GlfwWindow(const std::string& title, uint32_t width, uint32_t height);
		virtual ~GlfwWindow() override;

		virtual void pollEvents() override;

		virtual Backend backend() const override { return Backend::GLFW; }

		virtual void* hostHandle() const override { return m_Handle; }
		
		virtual FramebufferSize pixelSize() const;
		
		virtual void setTitle(const std::string& title) override;

		virtual void stop() override;

		virtual bool running() const override;
	private:
		void* m_Handle = nullptr;
	};

}
