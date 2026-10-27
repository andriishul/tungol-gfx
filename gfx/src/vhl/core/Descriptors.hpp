#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include "GraphicsCommon.hpp"

#include <vector>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT DescriptorSetLayoutList
	{
	public:
		~DescriptorSetLayoutList();
		DescriptorSetLayoutList(const Device& device);

		bool append(const uint32_t tag, const std::vector<DescriptorBinding>& bindings);
		bool remove(uint32_t tag);

		DescriptorSetLayoutHandle operator[](uint32_t tag) const;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	struct DescriptorPoolSize 
	{
		DescriptorType type;
		uint32_t descriptorCount;
	};


	class TUNGOL_EXPORT DescriptorPool 
	{
	public:
		DescriptorPool(const Device& device, uint32_t maxSets, const std::vector<DescriptorPoolSize>& poolSizes, bool updateAfterBind = false);
		~DescriptorPool();

		DescriptorSetHandle emplace(DescriptorSetLayoutHandle descriptorSetLayout);
		bool remove(DescriptorSetHandle setHandle);

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	class Buffer;
	class Image; 
	class ImageView;
	class Sampler;

	class TUNGOL_EXPORT DescriptorWriter
	{
	public:
		DescriptorWriter(const DescriptorSetHandle& set);
		~DescriptorWriter();

		DescriptorWriter& storageImage(const ImageView& imageView, uint32_t binding, ImageLayout layout = ImageLayout::General, uint32_t arrayIndex = 0);
		DescriptorWriter& sampledImage(const ImageView& imageView, const Sampler& sampler, uint32_t binding, ImageLayout layout = ImageLayout::General, uint32_t arrayIndex = 0);

		DescriptorWriter& uniformBuffer(const Buffer& buffer, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex = 0);
		DescriptorWriter& uniformBufferDynamic(const Buffer& buffer, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex = 0);

		DescriptorWriter& storageBuffer(const Buffer& buffer, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex = 0);
		DescriptorWriter& storageBufferDynamic(const Buffer& buffer, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex = 0);

		void apply(const Device& device); 
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};


	
}
