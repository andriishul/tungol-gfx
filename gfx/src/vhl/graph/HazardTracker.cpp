#include "HazardTracker.hpp"

#include <sstream>
#include <unordered_map>
#include <stdexcept>

namespace tungol::vhl {

	struct HazardTracker::Impl
	{
		std::unordered_map<Image*, ImageState> imageStates;
		std::unordered_map<Buffer*, BufferState> bufferStates;
	};

	HazardTracker::HazardTracker() : m_impl(new Impl)
	{
	}

	HazardTracker::~HazardTracker()
	{
		for (auto [img, state] : m_impl->imageStates) 
		{
			Trackable& trackable = *img;
			trackable.dispose = nullptr;
		}

		for (auto [buf, state] : m_impl->bufferStates)
		{
			Trackable& trackable = *buf;
			trackable.dispose = nullptr;
		}
	}

	ImageState& HazardTracker::imageState(Image& image)
	{
		if (auto it = m_impl->imageStates.find(&image); it != m_impl->imageStates.end())
		{
			return it->second;
		}

		std::ostringstream oss;
		oss << "[HazardTracker::imageState] State not found.\n"
			<< "  Image pointer : " << &image << "\n"
			<< "  Image handle  : " << image.handle().value << "\n";
		
		throw std::runtime_error(oss.str());
	}

	BufferState& HazardTracker::bufferState(Buffer& buf)
	{
		if (auto it = m_impl->bufferStates.find(&buf); it != m_impl->bufferStates.end())
		{
			return it->second;
		}

		std::ostringstream oss;
		oss << "[HazardTracker::bufferState] State not found.\n"
			<< "  Buffer pointer : " << &buf << "\n"
			<< "  Buffer handle  : " << buf.handle().value << "\n";

		throw std::runtime_error(oss.str());
	}

	bool HazardTracker::emplace(Image& img, const ImageState& state)
	{
		if (m_impl->imageStates.find(&img) != m_impl->imageStates.end())
			return false;

		ImageState newState = state;

		if (newState.subresources.empty())
		{
			newState.layerCount = img.layerCount();
			newState.levelCount = img.levelCount();
			newState.subresources.resize(newState.levelCount * newState.layerCount);
		}

		Trackable& trackable = img;
		trackable.dispose = [this](Trackable* tr)
		{
			if(auto it = m_impl->imageStates.find(static_cast<Image*>(tr)); it != m_impl->imageStates.end())
				m_impl->imageStates.erase(it);
		};

		m_impl->imageStates[&img] = newState;
		return true;
	}

	bool HazardTracker::emplace(Buffer& buf, const BufferState& state)
	{
		if (m_impl->bufferStates.find(&buf) != m_impl->bufferStates.end())
			return false;

		Trackable& trackable = buf;
		trackable.dispose = [this](Trackable* tr)
		{
			if (auto it = m_impl->bufferStates.find(static_cast<Buffer*>(tr)); it != m_impl->bufferStates.end())
				m_impl->bufferStates.erase(it);
		};

		BufferState newState = state;

		if (newState.ranges.empty())
		{
			newState.ranges.push_back(BufferRangeState{ AccessFlags::None, PipelineStageFlags::None, 0, buf.size() });
		}

		m_impl->bufferStates[&buf] = newState;
		
		return true;
	}

	bool HazardTracker::erase(Image& image)
	{
		auto it = m_impl->imageStates.find(&image);

		if (it == m_impl->imageStates.end())
			return false;

		Trackable& trackable = image;

		trackable.dispose = nullptr;
		m_impl->imageStates.erase(it);

		return true;
	}

	bool HazardTracker::erase(Buffer& buf)
	{
		auto it = m_impl->bufferStates.find(&buf);

		if (it == m_impl->bufferStates.end())
			return false;

		Trackable& trackable = buf;

		trackable.dispose = nullptr;
		m_impl->bufferStates.erase(it);

		return true;
	}
}
