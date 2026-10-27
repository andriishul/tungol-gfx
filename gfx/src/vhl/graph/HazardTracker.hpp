#pragma once

#include <vhl/core/images/Image.hpp>
#include <vhl/core/buffers/Buffer.hpp>

namespace tungol::vhl 
{
	struct SubresourceState
	{
		ImageLayout layout = ImageLayout::Undefined;
		AccessFlags access = AccessFlags::None;
		PipelineStageFlags stage = PipelineStageFlags::None;
	};

	struct ImageState
	{
		uint32_t levelCount = 1;
		uint32_t layerCount = 1;
		std::vector<SubresourceState> subresources;

		inline SubresourceState& at(uint32_t mip, uint32_t layer)
		{
			return subresources[mip * layerCount + layer];
		}

		inline const SubresourceState& at(uint32_t mip, uint32_t layer) const
		{
			return subresources[mip * layerCount + layer];
		}
	};

	struct BufferState 
	{
		AccessFlags access = AccessFlags::None;
		PipelineStageFlags stage = PipelineStageFlags::None;
	};

	class TUNGOL_EXPORT HazardTracker
	{
	public:
		HazardTracker();
		~HazardTracker();

		ImageState& imageState(Image& image);
		BufferState& bufferState(Buffer& buf);

		// Returns true if the resource was newly registered.
		bool emplace(Image& image, const ImageState& state = {});
		// Returns true if the resource was newly registered.
		bool emplace(Buffer& buf, const BufferState& state = {});

		
		bool erase(Image& image);
		bool erase(Buffer& image);
	private:
		friend class CommandEncoder;
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
