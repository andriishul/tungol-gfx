#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <optional>

namespace tungol::vhl
{
	struct AcquireResult
	{
		bool success = false;
		std::optional<uint32_t> imageIndex;
	};

	struct AcquireInfo 
	{
		const Semaphore* semaphore = nullptr;
		const Fence* fence = nullptr;
	};

	class TUNGOL_EXPORT Swapchain 
	{
	public:
		Swapchain(const Device& device, const struct SurfaceDesc& desc, const Swapchain* oldSwapchain = nullptr);
		~Swapchain();

		Image& colorImage(uint32_t index);
		const Image& colorImage(uint32_t index) const;
		uint32_t colorImageCount() const;

		uint32_t width() const;
		uint32_t height() const;
		Format colorFormat() const;

		// returns free swapchain image index
		AcquireResult acquire(const AcquireInfo&) const;

		SwapchainHandle handle() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
