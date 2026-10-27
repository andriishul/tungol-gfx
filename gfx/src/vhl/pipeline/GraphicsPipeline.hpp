#pragma once
#include "Pipeline.hpp"

namespace tungol::vhl 
{
	struct VertexInputBindingDesc
	{
		uint32_t binding;
		uint32_t stride;
		bool instanced = false;
	};

	struct VertexInputAttributeDesc
	{
		uint32_t location;
		uint32_t binding;
		Format format; 
		uint32_t offset;
	};

	struct Shaders
	{
		ShaderByteCode vertex;
		ShaderByteCode fragment;
	};

	struct VertexInputStageInfo
	{
		std::vector<VertexInputBindingDesc> bindings;
		std::vector<VertexInputAttributeDesc> attributes;
	};

	struct MultisampleStateDesc
	{
		SampleCount samples = SampleCount::One;
		bool alphaToCoverage = false;
	};


	struct DepthStateDesc
	{
		bool depthTest = true;
		bool depthWrite = true;
		CompareOp depthCompare = CompareOp::Less;
	};

	struct RasterizationStateDesc
	{
		PolygonMode polygonMode = PolygonMode::Fill;
		CullMode cullMode = CullMode::Back;
		FrontFace frontFace = FrontFace::CounterClockwise;

		bool depthBiasEnable = false;
		float depthBiasConstantFactor = 0.0f;
		float depthBiasSlopeFactor = 0.0f;
		float depthBiasClamp = 0.0f;
	};


	struct StencilOpState
	{
		StencilOp failOp;
		StencilOp passOp;
		StencilOp depthFailOp;
		CompareOp compareOp;

		uint32_t compareMask = 0xFF;
		uint32_t writeMask = 0xFF;
		uint32_t reference = 0;
	};

	struct StencilStateDesc
	{
		bool stencilTest = false;

		StencilOpState front;
		StencilOpState back;
	};

	


	struct GraphicsPipelineDesc
	{
		Shaders shaders;
		VertexInputStageInfo vertexInput;
		
		PrimitiveTopology topology;
		
		Viewport defaultViewport;
		Scissor defaultScissor;
		
		Format defaultColorFormat;
		Format defaultDepthFormat;
		Format defaultStencilFormat;

		MultisampleStateDesc multisample;
		DepthStateDesc depth;
		StencilStateDesc stencil;
		RasterizationStateDesc rasterization;
	};


	class TUNGOL_EXPORT GraphicsPipeline : public Pipeline
	{
	public:
		GraphicsPipeline(const Device& device);
		~GraphicsPipeline() override;

		GraphicsPipeline& init_layout(const PipelineLayoutDesc& desc);
		void build(const GraphicsPipelineDesc& desc);

		virtual PipelineHandle handle() const override;
		virtual PipelineLayoutHandle layoutHandle() const override;

		virtual PipelineType type() const override { return PipelineType::Graphics; }
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
