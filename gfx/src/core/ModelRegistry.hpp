#pragma once
#include <asset/ModelReader.hpp>

#include <sstream>

namespace tungol::core
{
	using ModelID = uint32_t;

	constexpr const ModelID EMPTY_MODEL = UINT32_MAX;

	class ModelRegistry 
	{
	public:
		inline const asset::ModelEntry& entry(ModelID id) const 
		{
			checkId(id);
			return m_entries[id];
		}
		inline const asset::Model& model(ModelID id) const 
		{
			checkId(id);
			return m_entries[id].model;
		}
		inline ModelID add(const asset::ModelEntry& mEntry) 
		{
			ModelID id = static_cast<ModelID>(m_entries.size());
			m_entries.push_back(mEntry);
			return id;
		}

		inline const auto& all() const { return m_entries; }
	private:
		std::vector<asset::ModelEntry> m_entries;

		inline void checkId(ModelID id) const
		{
			if (id >= m_entries.size())
			{
				std::ostringstream oss;
				oss << "[ModelRegistry::entry] Model ID " << id;
				oss << " is out of range. Valid IDs are 0 to ";
				oss << (m_entries.size() > 0 ? m_entries.size() - 1 : 0);
				oss << ". Did you forget to load the model?";

				throw std::out_of_range(oss.str());
			}
		}
	};
}
