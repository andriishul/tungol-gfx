#include "Sampler.hpp"
#include <vhl/utils/VkUtils.hpp>

namespace tungol::vhl {

	struct Sampler::Impl 
	{
		VkDevice logical = nullptr;
		VkSampler sampler = 0;


		Filter magFilter = Filter::Linear;
		Filter minFilter = Filter::Linear;
		MipmapMode mipmapMode = MipmapMode::Linear;

		AddressMode addressU = AddressMode::Repeat;
		AddressMode addressV = AddressMode::Repeat;
		AddressMode addressW = AddressMode::Repeat;

		BorderColor borderColor = BorderColor::FloatOpaqueBlack;

		bool enableComparison = false;
		bool enableAnisotropy = true;
	};

	Sampler::Sampler(const Device& device, const SamplerDesc& desc) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[Sampler::Sampler] Null device handle");


		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = utils::toVkFilter(desc.magFilter),
			.minFilter = utils::toVkFilter(desc.minFilter),
			.mipmapMode = utils::toVkSamplerMipmapMode(desc.mipmapMode),
			
			.addressModeU = utils::toVkSamplerAddressMode(desc.addressU),
			.addressModeV = utils::toVkSamplerAddressMode(desc.addressV),
			.addressModeW = utils::toVkSamplerAddressMode(desc.addressW),
			
			.mipLodBias = 0.0f,
			.anisotropyEnable = desc.enableAnisotropy ? VK_TRUE : VK_FALSE,
			.maxAnisotropy = desc.enableAnisotropy ? device.limits().image.maxSamplerAnisotropy : 1.0f,
			
			.compareEnable = desc.enableComparison ? VK_TRUE : VK_FALSE,
			.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,

			.minLod  = 0.0f,
			.maxLod = VK_LOD_CLAMP_NONE,
			.borderColor = utils::toVkBorderColor(desc.borderColor),
			.unnormalizedCoordinates = VK_FALSE
		};

		VK_CHECK(vkCreateSampler(m_impl->logical, &samplerInfo, nullptr, &m_impl->sampler));

		m_impl->magFilter = desc.magFilter;
		m_impl->minFilter = desc.minFilter;
		m_impl->mipmapMode = desc.mipmapMode;

		m_impl->addressU = desc.addressU;
		m_impl->addressV = desc.addressV;
		m_impl->addressW = desc.addressW;

		m_impl->borderColor = desc.borderColor;
		m_impl->enableComparison = desc.enableComparison;
		m_impl->enableAnisotropy = desc.enableAnisotropy;

	}

	Sampler::~Sampler()
	{
		if (m_impl && m_impl->logical && m_impl->sampler)
			vkDestroySampler(m_impl->logical, m_impl->sampler, nullptr);
	}

	Filter Sampler::magFilter() const
	{
		return m_impl->magFilter;
	}

	Filter Sampler::minFilter() const
	{
		return m_impl->minFilter;
	}

	MipmapMode Sampler::mipmapMode() const
	{
		return m_impl->mipmapMode;
	}

	AddressMode Sampler::addressU() const
	{
		return m_impl->addressU;
	}

	AddressMode Sampler::addressV() const
	{
		return m_impl->addressV;
	}

	AddressMode Sampler::addressW() const
	{
		return m_impl->addressW;
	}

	BorderColor Sampler::borderColor() const
	{
		return m_impl->borderColor;
	}

	bool Sampler::enableComparison() const
	{
		return m_impl->enableComparison;
	}

	bool Sampler::enableAnisotropy() const
	{
		return m_impl->enableAnisotropy;
	}

	SamplerHandle Sampler::handle() const
	{
		return SamplerHandle{ reinterpret_cast<uint64_t>(m_impl->sampler) };
	}

}

