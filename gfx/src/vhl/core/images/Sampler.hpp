#pragma once
#include <vhl/core/Device.hpp>

namespace tungol::vhl 
{
	struct SamplerDesc
	{
		Filter magFilter = Filter::Linear;
		Filter minFilter = Filter::Linear;
		MipmapMode mipmapMode = MipmapMode::Linear;

		AddressMode addressU = AddressMode::Repeat;
		AddressMode addressV = AddressMode::Repeat;
		AddressMode addressW = AddressMode::Repeat;

		BorderColor borderColor = BorderColor::FloatOpaqueBlack;
		
		// Minimal version to hide Vulkan boilerplate. LOD clamping and other
		// advanced fields can be added later when needed.

		bool enableComparison = false;
		bool enableAnisotropy = true;   // auto-caps at device maxAnisotropy
	};

	class TUNGOL_EXPORT Sampler 
	{
	public:
		Sampler(const Device& device, const SamplerDesc& desc = {});
		~Sampler();

		Filter magFilter() const;
		Filter minFilter() const;

		MipmapMode mipmapMode() const;

		AddressMode addressU() const;
		AddressMode addressV() const;
		AddressMode addressW() const;

		BorderColor borderColor() const;

		bool enableComparison() const;
		bool enableAnisotropy() const;

		SamplerHandle handle() const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
