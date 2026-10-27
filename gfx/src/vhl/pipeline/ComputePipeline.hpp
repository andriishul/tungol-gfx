#pragma once
#include "Pipeline.hpp"


namespace tungol::vhl 
{
	struct ComputePipelineDesc
	{
		ShaderByteCode compute;
	};

	class TUNGOL_EXPORT ComputePipeline : public Pipeline 
	{
	public:
		ComputePipeline(const Device& device);
		~ComputePipeline() override;

		ComputePipeline& init_layout(const PipelineLayoutDesc& desc);
		void build(const ComputePipelineDesc& desc);

		virtual PipelineHandle handle() const override;
		virtual PipelineLayoutHandle layoutHandle() const override;

		virtual PipelineType type() const override { return PipelineType::Compute; }
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
