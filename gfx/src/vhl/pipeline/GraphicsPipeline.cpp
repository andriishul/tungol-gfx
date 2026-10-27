#include "GraphicsPipeline.hpp"

#include <sstream>
#include <algorithm>

#include <vhl/utils/VkInspector.hpp>

namespace tungol::vhl 
{
	static VkStencilOpState toVkStencilOpState(StencilOpState state) 
	{
		return VkStencilOpState
		{
			.failOp = utils::toVkStencilOp(state.failOp),
			.passOp = utils::toVkStencilOp(state.passOp),

			.depthFailOp = utils::toVkStencilOp(state.depthFailOp),
			.compareOp = utils::toVkCompareOp(state.compareOp),
			.compareMask = state.compareMask,
			.writeMask = state.writeMask,
			.reference = state.reference
		};
	}

	static std::vector<VkVertexInputBindingDescription> toVectorVkVertexInputBindingDescription(const std::vector<VertexInputBindingDesc>& desc)
	{
		std::vector<VkVertexInputBindingDescription> vkDesc(desc.size());

		for (size_t i = 0; i < vkDesc.size(); ++i)
		{
			vkDesc[i].binding = desc[i].binding;
			vkDesc[i].stride = desc[i].stride;
			vkDesc[i].inputRate = desc[i].instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
		}

		return vkDesc;
	}

	static std::vector<VkVertexInputAttributeDescription> toVectorVkVertexInputAttributeDescription(const std::vector<VertexInputAttributeDesc>& attrs) 
	{
		std::vector<VkVertexInputAttributeDescription> vkAttr(attrs.size());

		for (size_t i = 0; i < vkAttr.size(); ++i)
		{
			vkAttr[i].binding = attrs[i].binding;
			vkAttr[i].format = utils::toVkFormat(attrs[i].format);
			vkAttr[i].location = attrs[i].location;
			vkAttr[i].offset = attrs[i].offset;
		}

		return vkAttr;
	}

	struct GraphicsPipeline::Impl 
	{
		VkDevice logical = VK_NULL_HANDLE;

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	};


	GraphicsPipeline::~GraphicsPipeline() 
	{
		if(m_impl && m_impl->logical && m_impl->graphicsPipeline)
			vkDestroyPipeline(m_impl->logical, m_impl->graphicsPipeline, nullptr);
		if (m_impl && m_impl->logical && m_impl->pipelineLayout)
			vkDestroyPipelineLayout(m_impl->logical, m_impl->pipelineLayout, nullptr);
	}

	GraphicsPipeline& GraphicsPipeline::init_layout(const PipelineLayoutDesc& desc) 
	{
		std::vector<VkPushConstantRange> vkRanges(desc.pushConstantRanges.size());
		for (size_t i = 0; i < vkRanges.size(); ++i) vkRanges[i] = utils::toVkPushConstantRange(desc.pushConstantRanges[i]);

		std::vector<VkDescriptorSetLayout> vkLayouts(desc.setLayouts.size());
		for (size_t i = 0; i < vkLayouts.size(); ++i)
		{
			if (uint64_t handle_value = desc.setLayouts[i].value; handle_value != 0)
				vkLayouts[i] = reinterpret_cast<VkDescriptorSetLayout>(handle_value);
			else throw std::runtime_error("[GraphicsPipeline::init_layout] Null descriptor set handle");
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
	
	void GraphicsPipeline::build(const GraphicsPipelineDesc& desc)
	{
		if (!m_impl) throw std::runtime_error("[GraphicsPipeline::build] Pipeline is nullptr.");
		if(!m_impl->pipelineLayout) throw std::runtime_error("[GraphicsPipeline::build] Pipeline layout is not initialized.");

		size_t vertexSize = desc.shaders.vertex.size() * sizeof(uint32_t);
		size_t fragmentSize = desc.shaders.fragment.size() * sizeof(uint32_t);

		if (!vertexSize) throw std::runtime_error("[GraphicsPipeline::build] Empty vertex shader bytecode");
		if (!fragmentSize) throw std::runtime_error("[GraphicsPipeline::build] Empty fragment shader bytecode");

		const uint32_t* vertexCode = desc.shaders.vertex.data();

		const uint32_t* fragmentCode = desc.shaders.fragment.data();

		VkPipelineShaderStageCreateInfo pipelineShaderStagesInfo[2];
		// VERTEX_STAGE
		pipelineShaderStagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStagesInfo[0].pNext = nullptr;
		pipelineShaderStagesInfo[0].flags = 0;
		pipelineShaderStagesInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineShaderStagesInfo[0].module = ShaderModule::create(m_impl->logical, vertexCode, vertexSize);
		pipelineShaderStagesInfo[0].pName = "main";
		pipelineShaderStagesInfo[0].pSpecializationInfo = nullptr;
		// FRAGMENT_STAGE
		pipelineShaderStagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStagesInfo[1].pNext = nullptr;
		pipelineShaderStagesInfo[1].flags = 0;
		pipelineShaderStagesInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineShaderStagesInfo[1].module = ShaderModule::create(m_impl->logical, fragmentCode, fragmentSize);
		pipelineShaderStagesInfo[1].pName = "main";
		pipelineShaderStagesInfo[1].pSpecializationInfo = nullptr;

		auto vertexBindingDescriptions = toVectorVkVertexInputBindingDescription(desc.vertexInput.bindings);
		auto vertexAttributeDescriptions = toVectorVkVertexInputAttributeDescription(desc.vertexInput.attributes);
		
		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = !vertexBindingDescriptions.empty() ? static_cast<uint32_t>(vertexBindingDescriptions.size()) : 0,
			.pVertexBindingDescriptions = !vertexBindingDescriptions.empty() ? vertexBindingDescriptions.data() : nullptr,

			.vertexAttributeDescriptionCount = !vertexAttributeDescriptions.empty() ? static_cast<uint32_t>(vertexAttributeDescriptions.size()) : 0,
			.pVertexAttributeDescriptions = !vertexAttributeDescriptions.empty() ? vertexAttributeDescriptions.data() : nullptr
		};


		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = utils::toVkPrimitiveTopology(desc.topology)
		};

		auto vkViewport = utils::toVkViewport(desc.defaultViewport);
		auto vkScissor = utils::toVkScissor(desc.defaultScissor);

		VkPipelineViewportStateCreateInfo viewportStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,

			.viewportCount = 1,
			.pViewports = &vkViewport,

			.scissorCount = 1,
			.pScissors = &vkScissor
		};
		
		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = utils::toVkPolygonMode(desc.rasterization.polygonMode),
			.cullMode = utils::toVkCullMode(desc.rasterization.cullMode), // VK_CULL_MODE_BACK_BIT, 
			.frontFace = utils::toVkFrontFace(desc.rasterization.frontFace) ,
			
			.depthBiasEnable = desc.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE,
			.depthBiasConstantFactor = desc.rasterization.depthBiasConstantFactor,
			.depthBiasClamp = desc.rasterization.depthBiasClamp,
			.depthBiasSlopeFactor = desc.rasterization.depthBiasSlopeFactor,
			
			.lineWidth = 1.0f
		};

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = utils::toVkSampleCountFlagBits(desc.multisample.samples), // BIT COUNT
			.sampleShadingEnable = VK_FALSE,
			.alphaToCoverageEnable = desc.multisample.alphaToCoverage
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = desc.depth.depthTest,
			.depthWriteEnable = desc.depth.depthWrite,
			.depthCompareOp = utils::toVkCompareOp(desc.depth.depthCompare),

			.stencilTestEnable = desc.stencil.stencilTest,
			.front = toVkStencilOpState(desc.stencil.front),
			.back = toVkStencilOpState(desc.stencil.back),
			
			.minDepthBounds = 0.0, // Lower bound used by depth bounds testing
			.maxDepthBounds = 1.0f, // Upper bound used by depth bounds testing
		};

		const bool hasColor = (desc.defaultColorFormat != Format::Undefined);

		VkPipelineColorBlendAttachmentState blendAttachment
		{
			.blendEnable = VK_FALSE, // No blending
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = hasColor ? 1U : 0U,
			.pAttachments = hasColor ? &blendAttachment : nullptr
		};

		VkDynamicState states[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			/*VK_DYNAMIC_STATE_DEPTH_BIAS,
			VK_DYNAMIC_STATE_BLEND_CONSTANTS,
			VK_DYNAMIC_STATE_LINE_WIDTH,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE*/
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = sizeof states / sizeof states[0],
			.pDynamicStates = states
		};

		VkFormat vkColorFormat = utils::toVkFormat(desc.defaultColorFormat);

		VkPipelineRenderingCreateInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = hasColor ? 1U : 0U,
			.pColorAttachmentFormats = hasColor ? &vkColorFormat : nullptr,
			.depthAttachmentFormat = utils::toVkFormat(desc.defaultDepthFormat),
			.stencilAttachmentFormat = utils::toVkFormat(desc.defaultStencilFormat)
		};

		
		VkGraphicsPipelineCreateInfo graphicsPipelineInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingInfo,

			.stageCount = 2,
			.pStages = pipelineShaderStagesInfo,
			.pVertexInputState = &vertexInputStateInfo,
			.pInputAssemblyState = &inputAssemblyStateInfo,
			.pTessellationState = nullptr, // nullptr for now
			.pViewportState = &viewportStateInfo,
			.pRasterizationState = &rasterizationStateInfo,
			.pMultisampleState = &multisampleStateInfo,
			.pDepthStencilState = &depthStencilStateInfo,
			.pColorBlendState = &colorBlendStateInfo,
		
			.pDynamicState = &dynamicStateInfo,

			.layout = m_impl->pipelineLayout,
			.basePipelineIndex = -1
		};
		
		// TODO implement PipelineCache
		VkResult result = vkCreateGraphicsPipelines(m_impl->logical, 0, 1, &graphicsPipelineInfo, nullptr, &m_impl->graphicsPipeline);

		for (size_t i = 0; i < 2; ++i)
		{
			vkDestroyShaderModule(m_impl->logical, pipelineShaderStagesInfo[i].module, nullptr);
		}

		VK_CHECK(result);
	}

	PipelineHandle GraphicsPipeline::handle() const
	{
		return PipelineHandle{ reinterpret_cast<uint64_t>(m_impl->graphicsPipeline) };
	}

	PipelineLayoutHandle GraphicsPipeline::layoutHandle() const
	{
		return PipelineLayoutHandle{ reinterpret_cast<uint64_t>(m_impl->pipelineLayout) };
	}

	GraphicsPipeline::GraphicsPipeline(const Device& device) : m_impl(new Impl)
	{
		if (uint64_t handle_value = device.handle().value; handle_value != 0)
			m_impl->logical = reinterpret_cast<VkDevice>(handle_value);
		else throw std::runtime_error("[GraphicsPipeline::GraphicsPipeline] Null device handle");
	}
}
