#pragma once
#include "AssetCache.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace tungol::core
{
	struct Instance
	{
		ModelID modelID = EMPTY_MODEL;
		math::mat4 worldTransform{};
		std::string clipName;
		float time = 0.0f;
		bool active = true;
	};

	struct ClipSlot
	{
	public:
		ClipSlot(std::string& clip, uint64_t& ver) : m_clipName(clip), m_version(ver) {}

		ClipSlot& operator=(const std::string& next)
		{
			if (m_clipName != next) { m_clipName = next; ++m_version; }
			return *this;
		}
		ClipSlot& operator=(std::string&& next)
		{
			if (m_clipName != next) { m_clipName = std::move(next); ++m_version; }
			return *this;
		}
		ClipSlot& operator=(const char* next)
		{
			if (m_clipName != next) { m_clipName = next; ++m_version; }
			return *this;
		}
		operator const std::string& () const { return m_clipName; }
	private:
		std::string& m_clipName;
		uint64_t& m_version;
	};


	struct InstanceView
	{
	public:
		InstanceView(Instance& inst, uint64_t& lv, uint64_t& cv) : m_instance(inst),
			m_layoutVersion(lv), m_clipVersion(cv), m_clipName(inst.clipName, cv)
		{}
		operator const Instance& () const { return m_instance; }
		
		ModelID& modelID() { return m_instance.modelID; }
		math::mat4& worldTransform() { return m_instance.worldTransform; }
		float& time() { return m_instance.time; }
		bool& active() { return m_instance.active; }
		ClipSlot& clipName() { return m_clipName; }
	private:
		ClipSlot m_clipName;
		Instance& m_instance;
		uint64_t& m_layoutVersion;
		uint64_t& m_clipVersion;
	};

	
	class Scene
	{
	public:
		Scene() = default;

		uint32_t addInstance(ModelID id, const math::mat4& transform)
		{
			Instance inst{};
			inst.modelID = id;
			inst.worldTransform = transform;
			return spawn(inst);
		}

		uint32_t spawn(const Instance& inst)
		{
			uint32_t idx;
			if (!m_freeSlots.empty())
			{
				idx = m_freeSlots.back();
				m_freeSlots.pop_back();
				m_instances[idx] = inst;
			}
			else
			{
				idx = static_cast<uint32_t>(m_instances.size());
				m_instances.push_back(inst);
			}
			m_instances[idx].active = true;
			++m_layoutVersion;
			return idx;
		}

		void despawn(uint32_t idx)
		{
			if (idx >= m_instances.size() || !m_instances[idx].active) return;
			m_instances[idx].active = false;
			m_freeSlots.push_back(idx);
			++m_layoutVersion;
		}

		void markLayoutDirty() { ++m_layoutVersion; }

		InstanceView instance(uint32_t idx)
		{
			return InstanceView{ m_instances[idx], m_layoutVersion, m_clipVersion };
		}

		const Instance& instance(uint32_t idx) const { return m_instances[idx]; }
		const std::vector<Instance>& instances() const { return m_instances; }

		// Changes when instances are spawned/despawned/re-modelled
		uint64_t layoutVersion() const { return m_layoutVersion; }

		// Changes when an instance's `clipName` changes
		uint64_t clipVersion() const { return m_clipVersion; }

	private:
		std::vector<uint32_t> m_freeSlots;
		std::vector<Instance> m_instances;
		uint64_t m_layoutVersion = 1;
		uint64_t m_clipVersion = 1;
	};
}