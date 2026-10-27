#pragma once
#include "ModelFormat.hpp"

#include <fstream>
#include <algorithm>

namespace tungol::asset 
{
	class ModelReader 
	{
	public:
		ModelReader() = default;

		inline ModelEntry read(const std::string& file)
		{
			ModelEntry mEntry{};

			std::string p = file.substr(0, file.find_last_of("/\\") + 1);
			std::replace(p.begin(), p.end(), '\\', '/');
			
			mEntry.path = p;
			auto& model = mEntry.model;

			std::ifstream fs(file, std::ios::ate | std::ios::binary);
			if (!fs.is_open()) throw std::runtime_error("[ModelReader] Failed to open: " + file);
			auto end = fs.tellg();
			fs.seekg(0, std::ios::beg);
			std::vector<char> bytes(end, 0);
			fs.read(bytes.data(), end);
			fs.close();

			ModelHeader header{};
			memcpy(&header, bytes.data(), sizeof header);

			if (header.magic != MODEL_MAGIC) throw std::runtime_error("[ModelReader] Model format not recognized.");

			if (!header.meshCount)
				throw std::runtime_error("[ModelReader] No meshes in: " + file);

			model.meshes.resize(header.meshCount);
			memcpy(model.meshes.data(), bytes.data() + header.meshTableOffset, header.meshCount * sizeof(MeshEntry));
			
			model.animations.resize(header.animationCount);
			memcpy(model.animations.data(), bytes.data() + header.animationTableOffset, header.animationCount * sizeof(AnimationHeader));

			model.channels.resize(header.channelTableSize / sizeof(AnimationChannel));
			memcpy(model.channels.data(), bytes.data() + header.channelTableOffset, header.channelTableSize);

			if (!header.staticVertexBlobSize && !header.skinnedVertexBlobSize)
				throw std::runtime_error("[ModelReader] No vertex data in: " + file);

			model.staticVertices.resize(header.staticVertexBlobSize / sizeof(StaticVertex));
			memcpy(model.staticVertices.data(), bytes.data() + header.staticVertexBlobOffset, header.staticVertexBlobSize);

			model.skinnedVertices.resize(header.skinnedVertexBlobSize / sizeof(SkinnedVertex));
			memcpy(model.skinnedVertices.data(), bytes.data() + header.skinnedVertexBlobOffset, header.skinnedVertexBlobSize);

			model.materials.resize(header.materialCount);
			memcpy(model.materials.data(), bytes.data() + header.materialTableOffset, header.materialCount * sizeof(MaterialEntry));

			model.bones.resize(header.boneCount);
			memcpy(model.bones.data(), bytes.data() + header.boneTableOffset, header.boneCount * sizeof(BoneEntry));

			model.frames.resize(header.keyframeBlobSize / sizeof(AnimationKey));
			memcpy(model.frames.data(), bytes.data() + header.keyframeBlobOffset, header.keyframeBlobSize);

			if (!header.indexBlobSize)
				throw std::runtime_error("[ModelReader] No indices in: " + file + " (indexed-only renderer)");

			model.indices.resize(header.indexBlobSize / sizeof(uint32_t));
			memcpy(model.indices.data(), bytes.data() + header.indexBlobOffset, header.indexBlobSize);
			
			
			return mEntry;
		}
	};
}
