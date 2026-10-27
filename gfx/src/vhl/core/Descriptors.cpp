#include "Descriptors.hpp"

#include "Device.hpp"

#include <vhl/utils/VkInspector.hpp>

#include <vhl/core/buffers/Buffer.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp> 
#include <vhl/core/images/Sampler.hpp>

#include <deque>
#include <algorithm>

namespace tungol::vhl 
{
	struct DescriptorSetLayoutList::Impl
	{
		struct Entry
		{
			uint32_t tag = 0;
			VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		};

		VkDevice logical = VK_NULL_HANDLE;

		std::vector<Entry> entries;
	};

	DescriptorSetLayoutList::~DescriptorSetLayoutList()
	{
		if (m_impl) 
		{
			for (const auto& entry : m_impl->entries)
			{
				vkDestroyDescriptorSetLayout(m_impl->logical, entry.layout, nullptr);
			}
		}
	}

	DescriptorSetLayoutList::DescriptorSetLayoutList(const Device& device) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[DescriptorSetLayoutList::DescriptorSetLayoutList] Null device handle");
	}

	bool DescriptorSetLayoutList::append(
		uint32_t tag,
		const std::vector<DescriptorBinding>& bindings)
	{
		if (std::find_if(m_impl->entries.begin(), m_impl->entries.end(), [tag](auto& entry) { return entry.tag == tag; }) == m_impl->entries.end())
		{
			bool anyUpdateAfterBind = false;
			std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());
			std::vector<VkDescriptorBindingFlags> vkBindingFlags(bindings.size());

			for (size_t i = 0; i < bindings.size(); ++i)
			{
				auto& binding = bindings[i];

				VkDescriptorSetLayoutBinding vk_binding
				{
					.binding = binding.binding,
					.descriptorType = utils::toVkDescriptorType(binding.type),
					.descriptorCount = binding.count,
					.stageFlags = utils::toVkShaderStageFlags(binding.stage)
				};

				auto& vkBindingFlag = vkBindingFlags[i];
				
				if (binding.partiallyBound)
				{
					vkBindingFlag |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
				}
				if (binding.updateAfterBind)
				{
					vkBindingFlag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
					anyUpdateAfterBind = true;
				}
				if (binding.variableCount)
				{
					vkBindingFlag |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
				}

				vkBindings[i] = vk_binding;
			}


			VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
				.bindingCount = static_cast<uint32_t>(vkBindingFlags.size()),
				.pBindingFlags = vkBindingFlags.data()
			};


			VkDescriptorSetLayoutCreateInfo setLayoutInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = &flagsInfo,
				.flags = anyUpdateAfterBind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : VkDescriptorSetLayoutCreateFlags(0),
				.bindingCount = static_cast<uint32_t>(vkBindings.size()),
				.pBindings = vkBindings.data()
			};

			VkDescriptorSetLayout layout = VK_NULL_HANDLE;

			VK_CHECK(vkCreateDescriptorSetLayout(m_impl->logical, &setLayoutInfo, nullptr, &layout));

			m_impl->entries.push_back(Impl::Entry{ tag, layout });

			return true;
		}

		return false;
	}
	bool DescriptorSetLayoutList::remove(uint32_t tag)
	{
		if (auto it = std::find_if(m_impl->entries.begin(), m_impl->entries.end(), [tag](auto& entry) { return entry.tag == tag; }); it != m_impl->entries.end())
		{
			vkDestroyDescriptorSetLayout(m_impl->logical, it->layout, nullptr);
			m_impl->entries.erase(it);
			return true;
		}
		return false;
	}
	DescriptorSetLayoutHandle DescriptorSetLayoutList::operator[](uint32_t tag) const
	{
		if (auto it = std::find_if(m_impl->entries.begin(), m_impl->entries.end(), [tag](auto& entry) { return entry.tag == tag; }); it != m_impl->entries.end())
		{
			return { reinterpret_cast<uint64_t>(it->layout) };
		}
		
		throw std::out_of_range("[DescriptorSetLayoutList::operator[]] Descriptor set: " + std::to_string(tag) + " not found!");
	}

	static std::vector<VkDescriptorPoolSize> toVectorVkDescriptorPoolSize(const std::vector<DescriptorPoolSize>& poolSizes) 
	{
		std::vector<VkDescriptorPoolSize> vkPoolSizes(poolSizes.size());

		for (size_t i = 0; i < vkPoolSizes.size(); ++i)
		{
			VkDescriptorPoolSize poolSize
			{
				.type = utils::toVkDescriptorType(poolSizes[i].type),
				.descriptorCount = poolSizes[i].descriptorCount
			};

			vkPoolSizes[i] = poolSize;
		}

		return vkPoolSizes;
	}

	struct DescriptorPool::Impl 
	{
		VkDevice logical = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		std::vector<DescriptorSetHandle> descriptors;
	};


	DescriptorPool::DescriptorPool(const Device& device, uint32_t maxSets, const std::vector<DescriptorPoolSize>& poolSizes, bool updateAfterBind) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[DescriptorPool::DescriptorPool] Null device handle");

		auto vkPoolSizes = toVectorVkDescriptorPoolSize(poolSizes);

		VkDescriptorPoolCreateInfo descriptorPoolInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | (updateAfterBind ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : VkDescriptorPoolCreateFlags(0)),
			.maxSets = maxSets,
			.poolSizeCount = (!vkPoolSizes.empty()) ? static_cast<uint32_t>(vkPoolSizes.size()) : 0,
			.pPoolSizes = (!vkPoolSizes.empty()) ? vkPoolSizes.data() : nullptr
		};

		VK_CHECK(vkCreateDescriptorPool(m_impl->logical, &descriptorPoolInfo, nullptr, &m_impl->descriptorPool));
	}

	DescriptorPool::~DescriptorPool() 
	{
		if (m_impl && m_impl->descriptorPool)
			vkDestroyDescriptorPool(m_impl->logical, m_impl->descriptorPool, nullptr);
	}

	DescriptorSetHandle DescriptorPool::emplace(DescriptorSetLayoutHandle handle)
	{
		VkDescriptorSet set = VK_NULL_HANDLE;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;

		if (uint64_t handle_value = handle.value; handle_value != 0)
			setLayout = reinterpret_cast<VkDescriptorSetLayout>(handle_value);
		else throw std::runtime_error("[DescriptorPool::emplace] Null descriptor set layout handle");


		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = m_impl->descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &setLayout
		};

		VK_CHECK(vkAllocateDescriptorSets(m_impl->logical, &descriptorSetAllocateInfo, &set));

		DescriptorSetHandle setHandle{ reinterpret_cast<uint64_t>(set) };
		m_impl->descriptors.push_back(setHandle);
		
		return setHandle;
	}

	bool DescriptorPool::remove(DescriptorSetHandle setHandle)
	{
		if (uint64_t handle_value = setHandle.value; handle_value != 0)
		{
			if (auto it = std::find_if(m_impl->descriptors.begin(), m_impl->descriptors.end(), [setHandle](DescriptorSetHandle handle) { return setHandle.value == handle.value;}); it != m_impl->descriptors.end())
			{
				VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(handle_value);

				vkFreeDescriptorSets(m_impl->logical, m_impl->descriptorPool, 1, &set);
				m_impl->descriptors.erase(it);
				return true;
			}

		}
		return false;
	}

	struct DescriptorWriter::Impl 
	{
		VkDescriptorSet set = VK_NULL_HANDLE;
		
		std::vector<VkWriteDescriptorSet> writeSetInfos;
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<VkDescriptorBufferInfo> bufferInfos;
	};

	DescriptorWriter::~DescriptorWriter() = default;


	DescriptorWriter& DescriptorWriter::storageImage(const ImageView& imageView, uint32_t binding, ImageLayout layout, uint32_t arrayIndex)
	{
		VkImageView vkImageView = 0;

		if (uint64_t handle_value = imageView.handle().value; handle_value != 0)
			vkImageView = reinterpret_cast<VkImageView>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::storageImage] Null image view handle");

		VkDescriptorImageInfo descImageInfo
		{
			.sampler = 0,
			.imageView = vkImageView,
			.imageLayout = utils::toVkImageLayout(layout)
		};

		m_impl->imageInfos.push_back(descImageInfo);

		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &m_impl->imageInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);

		return *this;
	}

	DescriptorWriter& DescriptorWriter::sampledImage(const ImageView& imageView, const Sampler& sampler, uint32_t binding, ImageLayout layout, uint32_t arrayIndex)
	{
		VkImageView vkImageView = 0;
		VkSampler vkSampler = 0;

		if (uint64_t handle_value = imageView.handle().value; handle_value != 0)
			vkImageView = reinterpret_cast<VkImageView>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::sampledImage] Null image view handle");

		if (uint64_t handle_value = sampler.handle().value; handle_value != 0)
			vkSampler = reinterpret_cast<VkSampler>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::sampledImage] Null sampler handle");


		VkDescriptorImageInfo descImageInfo
		{
			.sampler = vkSampler,
			.imageView = vkImageView,
			.imageLayout = utils::toVkImageLayout(layout)
		};

		m_impl->imageInfos.push_back(descImageInfo);

		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &m_impl->imageInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);


		return *this;
	}

	DescriptorWriter& DescriptorWriter::uniformBuffer(const Buffer& buf, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex)
	{
		VkBuffer vkBuffer = VK_NULL_HANDLE;

		if (buf.type() != BufferType::Uniform) throw std::runtime_error("[DescriptorWriter::uniformBuffer] Buffer is not UNIFORM!");

		if (uint64_t handle_value = buf.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::uniformBuffer] Null buffer handle");

		VkDescriptorBufferInfo descBufferInfo
		{
			.buffer = vkBuffer,
			.offset = offset,
			.range = range
		};


		m_impl->bufferInfos.push_back(descBufferInfo);


		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
			.pBufferInfo = &m_impl->bufferInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);

		return *this;
	}

	DescriptorWriter& DescriptorWriter::storageBuffer(const Buffer& buf, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex)
	{
		VkBuffer vkBuffer = 0;

		if (buf.type() != BufferType::Storage) throw std::runtime_error("[DescriptorWriter::storageBuffer] Buffer is not STORAGE!");

		if (uint64_t handle_value = buf.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::storageBuffer] Null buffer handle");

		VkDescriptorBufferInfo descBufferInfo
		{
			.buffer = vkBuffer,
			.offset = offset,
			.range = range
		};


		m_impl->bufferInfos.push_back(descBufferInfo);


		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &m_impl->bufferInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);

		return *this;
	}

	DescriptorWriter& DescriptorWriter::uniformBufferDynamic(const Buffer& buf, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex)
	{
		VkBuffer vkBuffer = 0;

		if (buf.type() != BufferType::Uniform) throw std::runtime_error("[DescriptorWriter::uniformBufferDynamic] Buffer is not UNIFORM!");

		if (uint64_t handle_value = buf.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::uniformBufferDynamic] Null buffer handle");

		VkDescriptorBufferInfo descBufferInfo
		{
			.buffer = vkBuffer,
			.offset = offset,
			.range = range
		};


		m_impl->bufferInfos.push_back(descBufferInfo);


		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pBufferInfo = &m_impl->bufferInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);

		return *this;
	}

	DescriptorWriter& DescriptorWriter::storageBufferDynamic(const Buffer& buf, uint32_t binding, uint64_t offset, uint64_t range, uint32_t arrayIndex)
	{
		VkBuffer vkBuffer = 0;

		if(buf.type() != BufferType::Storage) throw std::runtime_error("[DescriptorWriter::storageBufferDynamic] Buffer is not STORAGE!");

		if (uint64_t handle_value = buf.handle().value; handle_value != 0)
			vkBuffer = reinterpret_cast<VkBuffer>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::storageBufferDynamic] Null buffer handle");
		
		VkDescriptorBufferInfo descBufferInfo
		{
			.buffer = vkBuffer,
			.offset = offset,
			.range = range
		};

		m_impl->bufferInfos.push_back(descBufferInfo);

		VkWriteDescriptorSet writeSetInfo
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_impl->set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
			.pBufferInfo = &m_impl->bufferInfos.back()
		};

		m_impl->writeSetInfos.push_back(writeSetInfo);

		return *this;
	}

	DescriptorWriter::DescriptorWriter(const DescriptorSetHandle& handle) : m_impl(new Impl)
	{
		if (uint64_t handle_value = handle.value; handle_value != 0)
			m_impl->set = reinterpret_cast<VkDescriptorSet>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::DescriptorWriter] Null descriptor set handle");
	}


	void DescriptorWriter::apply(const Device& device) 
	{
		VkDevice logical = nullptr;

		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[DescriptorWriter::apply] Null device handle");
		
		vkUpdateDescriptorSets(logical, static_cast<uint32_t>(m_impl->writeSetInfos.size()), m_impl->writeSetInfos.data(), 0, nullptr);
	}

}
