#include "ComputePipeline.hpp"

#include <sstream>
#include <algorithm>

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{
	struct ComputePipeline::Impl 
	{
		VkDevice logical = VK_NULL_HANDLE;

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline computePipeline = VK_NULL_HANDLE;
	};

	ComputePipeline::ComputePipeline(const Device& device): m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[ComputePipeline::ComputePipeline] Null device handle");
	}
	ComputePipeline::~ComputePipeline()
	{
		if (m_impl && m_impl->logical && m_impl->computePipeline)
			vkDestroyPipeline(m_impl->logical, m_impl->computePipeline, nullptr);
		if (m_impl && m_impl->logical && m_impl->pipelineLayout)
			vkDestroyPipelineLayout(m_impl->logical, m_impl->pipelineLayout, nullptr);
	}

	ComputePipeline& ComputePipeline::init_layout(const PipelineLayoutDesc& desc)
	{
		std::vector<VkPushConstantRange> vkRanges(desc.pushConstantRanges.size());
		for (size_t i = 0; i < vkRanges.size(); ++i) vkRanges[i] = utils::toVkPushConstantRange(desc.pushConstantRanges[i]);

		std::vector<VkDescriptorSetLayout> vkLayouts(desc.setLayouts.size());
		for (size_t i = 0; i < vkLayouts.size(); ++i)
		{
			if (uint64_t handle_value = desc.setLayouts[i].value; handle_value != 0)
				vkLayouts[i] = reinterpret_cast<VkDescriptorSetLayout>(handle_value);
			else throw std::runtime_error("[ComputePipeline::init_layout] Null descriptor set handle");
		}
		VkPipelineLayoutCreateInfo pipelineLayoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,

			.setLayoutCount = !vkLayouts.empty() ? static_cast<uint32_t>(vkLayouts.size()) : 0,
			.pSetLayouts = !vkLayouts.empty() ? vkLayouts.data() : nullptr,

			.pushConstantRangeCount = !vkRanges.empty() ? static_cast<uint32_t>(vkRanges.size()) : 0,
			.pPushConstantRanges = !vkRanges.empty() ? vkRanges.data() : nullptr
		};

		VK_CHECK(vkCreatePipelineLayout(m_impl->logical, &pipelineLayoutInfo, nullptr, &m_impl->pipelineLayout));

		return *this;
	}
	void ComputePipeline::build(const ComputePipelineDesc& desc)
	{
		if (!m_impl) throw std::runtime_error("[ComputePipeline::build] Pipeline is nullptr.");
		if (!m_impl->pipelineLayout) throw std::runtime_error("[ComputePipeline::build] Pipeline layout is not initialized.");

		size_t computeSize = desc.compute.size() * sizeof(uint32_t);

		if (!computeSize) throw std::runtime_error("[ComputePipeline::build] Empty compute shader bytecode");

		const uint32_t* computeCode = desc.compute.data();

		VkPipelineShaderStageCreateInfo pipelineShaderStageInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = ShaderModule::create(m_impl->logical, computeCode, computeSize),
			.pName = "main"
		};

		VkComputePipelineCreateInfo computePipelineInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = pipelineShaderStageInfo,
			.layout = m_impl->pipelineLayout
		};

		VkResult result = vkCreateComputePipelines(m_impl->logical, 0, 1, &computePipelineInfo, nullptr, &m_impl->computePipeline);

		vkDestroyShaderModule(m_impl->logical, pipelineShaderStageInfo.module, nullptr);

		VK_CHECK(result);
	}
	PipelineHandle ComputePipeline::handle() const
	{
		return PipelineHandle{ reinterpret_cast<uint64_t>(m_impl->computePipeline) };
	}
	PipelineLayoutHandle ComputePipeline::layoutHandle() const
	{
		return PipelineLayoutHandle{ reinterpret_cast<uint64_t>(m_impl->pipelineLayout) };
	}
}
