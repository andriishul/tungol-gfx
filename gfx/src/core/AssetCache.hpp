#pragma once
#include <core/TungolExport.hpp>

#include <asset/ModelReader.hpp>
#include <asset/BcTextureReader.hpp>

#include <unordered_map>
#include <future>

namespace tungol::core
{
	using ModelID = uint64_t;

	constexpr const ModelID EMPTY_MODEL = UINT64_MAX;

	struct ModelRecord 
	{
		asset::ModelEntry entry;
		std::unordered_map<std::string, asset::BcTexture> textures; 
	};

	struct FutureRecord 
	{
		std::future<ModelRecord> future;
	};


	class TUNGOL_EXPORT AssetCache
	{
	public:	
		AssetCache() = default;
		AssetCache(const AssetCache&) = delete;
		AssetCache& operator=(const AssetCache&) = delete;

		AssetCache(AssetCache&&) = default;
		AssetCache& operator=(AssetCache&&) = default;

		ModelID loadAsync(const std::string& path);
		void unload(ModelID id);

		bool isReady(ModelID id) const;
		const ModelRecord& record(ModelID id) const;

		void poll();
	private:
		std::unordered_map<ModelID, ModelRecord> m_records;
		std::unordered_map<ModelID, FutureRecord> m_pendings;

		void load(ModelRecord& record, const std::string& dir, const std::string& file);

		uint64_t hashId(const std::string& path) const;
	};
}
