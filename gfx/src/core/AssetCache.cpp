#include "AssetCache.hpp"

#include <sstream>

namespace tungol::core
{
	void AssetCache::load(ModelRecord& record, const std::string& dir, const std::string& file)
	{
		if (file.empty() || !file[0]) return;
		const std::string path = dir + file;
		if (record.textures.contains(path)) return;
		record.textures.emplace(path, asset::load_bc_texture(path));
	}
	uint64_t AssetCache::hashId(const std::string& path) const
	{
		return std::hash<std::string>{}(path);
	}

	ModelID AssetCache::loadAsync(const std::string& path)
	{
		ModelID mID = hashId(path);

		if (m_records.contains(mID) || m_pendings.contains(mID)) return mID;
		
		FutureRecord futRec;
		futRec.future = std::async(std::launch::async, [this, path]()
		{
			ModelRecord record;
			asset::ModelReader reader;

			record.entry = reader.read(path);

			const std::string dir = record.entry.path;

			for (auto& mat : record.entry.model.materials) 
			{
				load(record, dir, mat.albedoPath);
				load(record, dir, mat.emissivePath);
				load(record, dir, mat.mrPath);
				load(record, dir, mat.normalPath);
			}

			return record;
		});

		m_pendings[mID] = std::move(futRec);

		return mID;
	}

	void AssetCache::unload(ModelID id)
	{
		if (m_pendings.contains(id)) return;
		m_records.erase(id);
	}

	bool AssetCache::isReady(ModelID id) const
	{
		return m_records.contains(id);
	}

	const ModelRecord& AssetCache::record(ModelID id) const
	{
		if (auto it = m_records.find(id); it != m_records.end()) 
		{
			return it->second;
		}

		std::ostringstream ss;
		ss << "[AssetCache::record] record on modelID: " << id << "not found.";
		throw std::runtime_error(ss.str());
	}

	void AssetCache::poll()
	{
		for (auto it = m_pendings.begin(); it != m_pendings.end();)
		{
			if (it->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			{
				ModelID id = it->first;
				m_records[id] = it->second.future.get();

				it = m_pendings.erase(it);
			}
			else ++it;
		}
	}
	
}