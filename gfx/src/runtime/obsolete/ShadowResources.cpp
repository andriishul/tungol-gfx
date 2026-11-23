#include "ShadowResources.hpp"

#include "helper/CatchGraphicsError.hpp"

#include "RenderConfig.hpp"
#include "RenderKernel.hpp"

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>

#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <stdexcept>

namespace tungol::runtime
{

	struct ShadowResources::Impl 
	{
		Impl(RenderKernel& kern);
		~Impl();

		RenderKernel& kernel;

		std::unique_ptr<vhl::Image> image;
		std::unique_ptr<vhl::ImageView> view;
		std::unique_ptr<vhl::Sampler> sampler;
		vhl::DescriptorSetHandle set{};
	};

	ShadowResources::Impl::Impl(RenderKernel& kern) : kernel(kern)
	{	
		auto& tracker = kernel.tracker();
		auto& descriptorPool = kernel.descriptorPool();
		auto& layouts = kernel.layouts();
		auto& allocator = kernel.allocator();

		const auto& device = kernel.context().activeDevice();

		const vhl::ImageDesc imageDesc
		{
			.format = main::k_ShadowMapFormat,
			.width = main::k_ShadowMapResolution,
			.height = main::k_ShadowMapResolution,
			.usage = vhl::ImageUsageFlags::DepthStencil | vhl::ImageUsageFlags::Sampled
		};

		image = std::make_unique<vhl::Image>(device, allocator, imageDesc);
		tracker.emplace(*image);

		const vhl::ImageViewDesc viewDesc
		{
			.format = main::k_ShadowMapFormat,
			.aspect = vhl::ImageAspectFlags::Depth
		};


		view = std::make_unique<vhl::ImageView>(device, *image, viewDesc);

		const vhl::SamplerDesc samplerDesc
		{
			.magFilter = vhl::Filter::Linear,
			.minFilter = vhl::Filter::Linear,
			.mipmapMode = vhl::MipmapMode::Nearest,

			.addressU = vhl::AddressMode::ClampToEdge,
			.addressV = vhl::AddressMode::ClampToEdge,
			.addressW = vhl::AddressMode::ClampToEdge,

			.borderColor = vhl::BorderColor::FloatOpaqueWhite,
			.enableComparison = true,
			.enableAnisotropy = false
		};

		sampler = std::make_unique<vhl::Sampler>(device, samplerDesc);

		set = descriptorPool.emplace(layouts[5]);

		vhl::DescriptorWriter writer(set);
		writer.sampledImage(*view, *sampler, 0, vhl::ImageLayout::ShaderReadOnly)
			.apply(device);
	}

	ShadowResources::Impl::~Impl() 
	{
		try 
		{
			kernel.context().activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR

		kernel.descriptorPool().remove(set);
		sampler.reset();
		view.reset();
		image.reset();
	}


	ShadowResources::ShadowResources(RenderKernel& kern) : m_impl(new Impl(kern))
	{
	}
	ShadowResources::~ShadowResources() = default;

	vhl::Image& ShadowResources::image() const
	{
		if (!m_impl->image) throw std::runtime_error("[ShadowResources::image] Not initialized. ShadowResources construction failed.");
		return *m_impl->image;
	}
	vhl::ImageView& ShadowResources::view() const
	{
		if (!m_impl->view) throw std::runtime_error("[ShadowResources::view] Not initialized. ShadowResources construction failed.");
		return *m_impl->view;
	}
	const vhl::DescriptorSetHandle& ShadowResources::set() const
	{
		if (!m_impl->set.value) throw std::runtime_error("[ShadowResources::set] Not initialized. ShadowResources construction failed.");
		return m_impl->set;
	}
	
}
