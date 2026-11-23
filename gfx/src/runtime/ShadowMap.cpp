#include "ShadowMap.hpp"
#include "internal/KernelInternal.hpp"

#include <vhl/core/Allocator.hpp>
#include <vhl/core/GraphicsContext.hpp>
#include <vhl/graph/HazardTracker.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/Descriptors.hpp>

namespace tungol::runtime 
{
	struct ShadowMap::Impl 
	{
		Impl(RenderKernel& k, const vhl::Format fmt, uint32_t resolution);
		~Impl();
		
		std::unique_ptr<vhl::Image> image;
		std::unique_ptr<vhl::ImageView> view;
		std::unique_ptr<vhl::Sampler> sampler;
		vhl::DescriptorSetHandle set;

		vhl::DescriptorPool& descriptorPool;
	};


	ShadowMap::Impl::Impl(RenderKernel& k, const vhl::Format fmt, uint32_t resolution) : 
		descriptorPool(KernelInternal::descriptorPool(k))
	{
		auto& tracker = KernelInternal::tracker(k);
		auto& allocator = KernelInternal::allocator(k);
		auto& device = KernelInternal::context(k).activeDevice();
		auto& layouts = KernelInternal::layouts(k);


		const vhl::ImageDesc imageDesc
		{
			.format = fmt,
			.width = resolution,
			.height = resolution,
			.usage = vhl::ImageUsageFlags::DepthStencil | vhl::ImageUsageFlags::Sampled
		};

		image = std::make_unique<vhl::Image>(device, allocator, imageDesc);
		tracker.emplace(*image);

		const vhl::ImageViewDesc viewDesc
		{
			.format = fmt,
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


	ShadowMap::Impl::~Impl() 
	{
		descriptorPool.remove(set);
		
		sampler.reset();
		view.reset();
		image.reset();
	}



	ShadowMap::~ShadowMap() = default;

	ShadowMap::ShadowMap(RenderKernel& kernel, const vhl::Format fmt, uint32_t resolution) : m_impl(new Impl(kernel, fmt, resolution))
	{
		
	}

	vhl::Image& ShadowMap::image()
	{
		return *m_impl->image;
	}

	vhl::ImageView& ShadowMap::view()
	{
		return *m_impl->view;
	}

	const vhl::DescriptorSetHandle& ShadowMap::set() const
	{
		return m_impl->set;
	}

}