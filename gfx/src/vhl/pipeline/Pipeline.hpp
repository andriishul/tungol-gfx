#pragma once
#include <vhl/commands/CommandBuffer.hpp>
#include <vhl/core/Descriptors.hpp>

namespace tungol::vhl 
{
	using ShaderByteCode = std::vector<uint32_t>;

	struct PipelineLayoutDesc
	{
		std::vector<DescriptorSetLayoutHandle> setLayouts;
		std::vector<PushConstantRange> pushConstantRanges;
	};

	enum class PipelineType : uint32_t
	{
		Graphics, Compute
	};

	class TUNGOL_EXPORT Pipeline 
	{
	public:
		

		virtual ~Pipeline() = default;

		virtual PipelineHandle handle() const = 0;
		virtual PipelineLayoutHandle layoutHandle() const = 0;

		virtual PipelineType type() const = 0;
	};
}
