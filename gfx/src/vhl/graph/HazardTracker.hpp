#pragma once

#include <vhl/core/images/Image.hpp>
#include <vhl/core/buffers/Buffer.hpp>

#include <algorithm>

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

	struct BufferRangeState 
	{
		AccessFlags access = AccessFlags::None;
		PipelineStageFlags stage = PipelineStageFlags::None;

		uint64_t offset = 0;
		uint64_t size = BufferWholeSize;
	};

	struct BufferState
	{
		std::vector<BufferRangeState> ranges;

		void emplaceRange(
			uint64_t offset, uint64_t size, 
			AccessFlags access = AccessFlags::None, 
			PipelineStageFlags stage = PipelineStageFlags::None)
		{
			const uint64_t end = offset + size;
			std::vector<BufferRangeState> next;
			next.reserve(ranges.size() + 2);

			for (const auto& r : ranges)
			{
				const uint64_t rEnd = r.offset + r.size;

				if (rEnd <= offset) { next.push_back(r); continue; }     
				if (r.offset >= end) { next.push_back(r); continue; }   

				if (r.offset < offset)
				{                                 
					next.push_back(BufferRangeState{ r.access, r.stage, r.offset, offset - r.offset });
				}

				if (rEnd > end)
				{                                         
					next.push_back(BufferRangeState{ r.access, r.stage, end, rEnd - end });
				}
			}

			next.push_back({ access, stage, offset, size });

			std::sort(next.begin(), next.end(), [](const BufferRangeState& a, const BufferRangeState& b)
			{ 
				return a.offset < b.offset; 
			});

			for (size_t i = 1; i < next.size(); )
			{
				auto& a = next[i - 1];
				auto& b = next[i];
				if (a.offset + a.size == b.offset && a.access == b.access && a.stage == b.stage)
				{
					a.size += b.size;
					next.erase(next.begin() + i);
				}
				else ++i;
			}

			ranges = std::move(next);
		}
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
