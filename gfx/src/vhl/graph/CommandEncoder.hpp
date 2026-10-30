#pragma once
#include <vhl/graph/CommandQueue.hpp>
#include <vhl/commands/CommandRecorder.hpp>

#include <functional>

namespace tungol::vhl 
{
	class TUNGOL_EXPORT CommandEncoder
	{
	public:
		CommandEncoder(CommandQueue& queue, class HazardTracker& tracker);
		~CommandEncoder();

		struct TUNGOL_EXPORT PassBuilder
		{
		public:
			PassBuilder& useImage(Image& image, UsageIntent usage, uint32_t baseMip = 0, uint32_t mipCount = RemainingMipLevels,uint32_t baseLayer = 0, uint32_t layerCount = RemainingArrayLayers);
			PassBuilder& useBuffer(Buffer& buffer, UsageIntent usage, uint64_t offset = 0, uint64_t size = BufferWholeSize);
		private:
			friend class CommandEncoder; 
			PassBuilder(CommandEncoder* encoder, uint32_t index); 
			CommandEncoder* m_encoder = nullptr; 
			uint32_t m_index = 0;
		};

		void setDrawable(Image& image);

		PassBuilder encodePass(const std::function<void(CommandRecorder& recorder)>& callback);

		void commit();
	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
