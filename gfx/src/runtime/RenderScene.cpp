#include "RenderScene.hpp"

#include "helper/CatchGraphicsError.hpp"

#include "RenderKernel.hpp"
#include "RenderConfig.hpp"

#include <core/ModelRegistry.hpp>

#include "Texture.hpp"

#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Allocator.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/CommandQueue.hpp>
#include <vhl/graph/HazardTracker.hpp>

#include <vhl/core/buffers/VertexBuffer.hpp>
#include <vhl/core/buffers/UniformBuffer.hpp>
#include <vhl/core/buffers/IndexBuffer.hpp>
#include <vhl/core/buffers/IndirectBuffer.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/commands/CopyContext.hpp>

#include <vhl/commands/DrawIndirectCommand.hpp>

#include <filesystem>

namespace tungol::runtime
{

    namespace
    {
        struct ModelBase
        {
            uint32_t staticBase = 0;
            uint32_t skinnedBase = 0;
            uint32_t indexBase = 0;
            uint32_t materialBase = 0;
        };

        struct alignas(16) InstanceData
        {
            math::mat4 modelMatrix;
            uint32_t materialIndex = UINT32_MAX;
            uint32_t boneOffset = UINT32_MAX;
        };

        struct alignas(16) MaterialData
        {
            math::vec4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            float metallic = 0.0f;
            float roughness = 0.5f;
            uint32_t albedoIdx = UINT32_MAX;
            uint32_t normalIdx = UINT32_MAX;
            uint32_t mrIdx = UINT32_MAX;
            uint32_t emissiveIdx = UINT32_MAX;
        };


    }

    static void computeSceneBounds(const core::Scene& scene, const core::ModelRegistry& models, math::vec3& outMin, math::vec3& outMax)
    {
        outMin = { +1e30f, +1e30f, +1e30f };
        outMax = { -1e30f, -1e30f, -1e30f };

        for (const auto& inst : scene.instances())
        {
            const auto& model = models.model(inst.modelID);
            const math::mat4& mtx = inst.transform;

            for (const auto& v : model.staticVertices)
            {
                math::vec4 w = mtx * math::vec4(v.position.x, v.position.y, v.position.z, 1.0f);
                outMin.x = std::min(outMin.x, w.x);
                outMin.y = std::min(outMin.y, w.y);
                outMin.z = std::min(outMin.z, w.z);
                outMax.x = std::max(outMax.x, w.x);
                outMax.y = std::max(outMax.y, w.y);
                outMax.z = std::max(outMax.z, w.z);
            }

            for (const auto& v : model.skinnedVertices)
            {
                math::vec4 w = mtx * math::vec4(v.position.x, v.position.y, v.position.z, 1.0f);
                outMin.x = std::min(outMin.x, w.x);
                outMin.y = std::min(outMin.y, w.y);
                outMin.z = std::min(outMin.z, w.z);
                outMax.x = std::max(outMax.x, w.x);
                outMax.y = std::max(outMax.y, w.y);
                outMax.z = std::max(outMax.z, w.z);
            }
        }

        if (outMin.x > outMax.x)
        {
            outMin = { -1.0f, -1.0f, -1.0f };
            outMax = { 1.0f,  1.0f,  1.0f };
            return;
        }

        const math::vec3 center = (outMin + outMax) * 0.5f;
        const math::vec3 half = (outMax - outMin) * 0.5f * 1.2f;
        outMin = center - half;
        outMax = center + half;
    }

    struct RenderScene::Impl
    {
        RenderKernel& kernel;
        core::ModelRegistry& models;

        ~Impl();

        math::vec3 sceneMin{ +1e30f, +1e30f, +1e30f };
        math::vec3 sceneMax{ -1e30f, -1e30f, -1e30f };

        uint32_t staticDrawCount = 0, skinnedDrawCount = 0;

        std::unique_ptr<vhl::VertexBuffer> staticVB, skinnedVB;
        std::unique_ptr<vhl::IndexBuffer> indexBuf;

        vhl::DescriptorSetHandle objectSets[RenderKernel::FramesInFlight];
        std::unique_ptr<vhl::StorageBuffer> instanceSBs[RenderKernel::FramesInFlight];
        std::unique_ptr<vhl::StagingBuffer> instanceTBs[RenderKernel::FramesInFlight];

        std::unique_ptr<vhl::StorageBuffer> materialSB;

        std::unique_ptr<vhl::IndirectBuffer> indirectBuf;

        std::vector<ModelBase> modelBases;

        std::vector<InstanceData> instancesData;

        std::unordered_map<std::string, uint32_t> texturesLoaded;
        std::unique_ptr<TextureRegistry> textures;
        
        std::unique_ptr<AnimationResources> animationResources;
        std::unique_ptr<ShadowResources> shadowResources;

        uint32_t registerTexture(vhl::CopyContext& cpy, vhl::Format fmt, const std::string& path, int ch, bool srgb);

        void commit(const core::Scene& scene);

        uint8_t* build(const core::Scene& scene);

        void fillInstances(const core::Scene& scene, std::vector<InstanceData>& out) const;
        void fillIndirectCommands(const core::Scene& scene, std::vector<vhl::DrawIndexedIndirectCommand>& outStatic, std::vector<vhl::DrawIndexedIndirectCommand>& outSkinned) const;
    };

    RenderScene::Impl::~Impl()
    {
        try
        {
            kernel.context().activeDevice().wait();
        }
        CATCH_GRAPHICS_ERROR

        shadowResources.reset();
        animationResources.reset();

        textures.reset();

        staticVB.reset();
        skinnedVB.reset();
        indexBuf.reset();

        for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
        {
            kernel.descriptorPool().remove(objectSets[i]);
            objectSets[i].value = 0;

            instanceTBs[i].reset();
            instanceSBs[i].reset();
        }

        materialSB.reset();
        indirectBuf.reset();
        staticDrawCount = 0;
        skinnedDrawCount = 0;
    }

    uint32_t RenderScene::Impl::registerTexture(vhl::CopyContext& batch, vhl::Format fmt, const std::string& path, int ch, bool srgb)
    {
        auto it = texturesLoaded.find(path);
        if (it == texturesLoaded.cend())
        {
            Texture tex = loadTexture(kernel.context().activeDevice(), kernel.allocator(), batch, path, fmt, ch, srgb);
            auto then = std::chrono::steady_clock::now();

            batch.flush();
            auto after = std::chrono::steady_clock::now();

            std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(after - then);
            uint32_t idx = textures->add(std::move(tex));

            texturesLoaded[path] = idx;
            return idx;
        }
        else return it->second;
    }

    void RenderScene::Impl::commit(const core::Scene& scene)
    {
        auto& device = kernel.context().activeDevice();
        auto& allocator = kernel.allocator();
        auto& descriptorPool = kernel.descriptorPool();
        auto& layouts = kernel.layouts();
        auto& scheduler = kernel.mainQueue();
        auto& tracker = kernel.tracker();
     
        textures = std::make_unique<TextureRegistry>(device, descriptorPool, layouts, main::k_MaxBindlessTexturesPerScene);

   
        vhl::CopyContext cpyContext(device, allocator, kernel.mainQueue(), kernel.tracker());

        const auto& entries = models.all();

        if (entries.empty()) throw std::runtime_error("[RenderScene::commit] Cannot commit: ModelRegistry is empty. Did you load any models?");

        modelBases.resize(entries.size());
        computeSceneBounds(scene, models, sceneMin, sceneMax);

        uint64_t totalStatic = 0, totalSkinned = 0, totalIndices = 0, totalMaterials = sizeof(MaterialData);
        for (const auto& entry : entries)
        {
            const auto& m = entry.model;
            totalStatic += m.staticVertices.size() * sizeof(asset::StaticVertex);
            totalSkinned += m.skinnedVertices.size() * sizeof(asset::SkinnedVertex);
            totalIndices += m.indices.size() * sizeof(uint32_t);
            totalMaterials += m.materials.size() * sizeof(MaterialData);
        }

        const uint64_t staticBytes = std::max<uint64_t>(totalStatic, sizeof(asset::StaticVertex));
        const uint64_t skinnedBytes = std::max<uint64_t>(totalSkinned, sizeof(asset::SkinnedVertex));
        const uint64_t indexBytes = std::max<uint64_t>(totalIndices, sizeof(uint32_t));

        staticVB = std::make_unique<vhl::VertexBuffer>(device, allocator, staticBytes);
        skinnedVB = std::make_unique<vhl::VertexBuffer>(device, allocator, skinnedBytes);
        indexBuf = std::make_unique<vhl::IndexBuffer>(device, allocator, indexBytes);
        materialSB = std::make_unique<vhl::StorageBuffer>(device, allocator, totalMaterials);

        if (!totalStatic)
        {
            tracker.emplace(*staticVB);
        }

        if (!totalSkinned)
        {
            tracker.emplace(*skinnedVB);
        } 
        
        if (!totalIndices)
        {
            tracker.emplace(*indexBuf);
        }

        std::vector<MaterialData> allMaterials;
        allMaterials.reserve(totalMaterials / sizeof(MaterialData));
        allMaterials.push_back(MaterialData{});

        uint64_t staticOffset = 0, skinnedOffset = 0, indexOffset = 0;
        uint32_t staticBase = 0, skinnedBase = 0, indexBase = 0, materialBase = 1;

        for (uint32_t id = 0; id < entries.size(); ++id)
        {
            const auto& entry = entries[id];
            const auto& model = entry.model;

            modelBases[id] = ModelBase{ staticBase, skinnedBase, indexBase, materialBase };

            if (!model.staticVertices.empty())
            {
                cpyContext.upload(*staticVB, model.staticVertices.data(), model.staticVertices.size() * sizeof(asset::StaticVertex), staticOffset);
                staticOffset += model.staticVertices.size() * sizeof(asset::StaticVertex);
                staticBase += static_cast<uint32_t>(model.staticVertices.size());
            }
            if (!model.skinnedVertices.empty())
            {
                cpyContext.upload(*skinnedVB, model.skinnedVertices.data(), model.skinnedVertices.size() * sizeof(asset::SkinnedVertex), skinnedOffset);
                skinnedOffset += model.skinnedVertices.size() * sizeof(asset::SkinnedVertex);
                skinnedBase += static_cast<uint32_t>(model.skinnedVertices.size());
            }
            if (!model.indices.empty())
            {
                cpyContext.upload(*indexBuf, model.indices.data(), model.indices.size() * sizeof(uint32_t), indexOffset);
                indexOffset += model.indices.size() * sizeof(uint32_t);
                indexBase += static_cast<uint32_t>(model.indices.size());
            }

            cpyContext.flush();

            for (const auto& mat : model.materials)
            {
                MaterialData md{};
                md.baseColor = mat.baseColor;
                md.metallic = mat.metallic;
                md.roughness = mat.roughness;
                md.albedoIdx = md.normalIdx = md.mrIdx = md.emissiveIdx = UINT32_MAX;

                if (mat.albedoPath[0] != 0 && std::filesystem::exists(entry.path + mat.albedoPath))
                    md.albedoIdx = registerTexture(cpyContext, vhl::Format::Rgba8Srgb, entry.path + mat.albedoPath, 4, true);
                if (mat.normalPath[0] != 0 && std::filesystem::exists(entry.path + mat.normalPath))
                    md.normalIdx = registerTexture(cpyContext, vhl::Format::Rgba8Unorm, entry.path + mat.normalPath, 4, false);
                if (mat.mrPath[0] != 0 && std::filesystem::exists(entry.path + mat.mrPath))
                    md.mrIdx = registerTexture(cpyContext, vhl::Format::Rg8Unorm, entry.path + mat.mrPath, 2, false);
                if (mat.emissivePath[0] != 0 && std::filesystem::exists(entry.path + mat.emissivePath))
                    md.emissiveIdx = registerTexture(cpyContext, vhl::Format::Rgba8Srgb, entry.path + mat.emissivePath, 4, true);

                allMaterials.push_back(md);
            }
           

            materialBase += static_cast<uint32_t>(model.materials.size());

        }

        cpyContext.upload(*materialSB, allMaterials.data(), allMaterials.size() * sizeof(MaterialData));
        cpyContext.flush();

        shadowResources = std::make_unique<ShadowResources>(kernel);
        animationResources = std::make_unique<AnimationResources>(kernel);
        {
            animationResources->buildStatic(scene, models, cpyContext);
            cpyContext.flush();
        }

        const auto& instances = scene.instances();

        uint32_t totalMeshInstances = 0;
        for (const auto& inst : instances)
            totalMeshInstances += static_cast<uint32_t>(models.model(inst.modelID).meshes.size());

        std::vector<InstanceData> instancesData;
        instancesData.reserve(totalMeshInstances);
        fillInstances(scene, instancesData);

        std::vector<vhl::DrawIndexedIndirectCommand> staticArgs, skinnedArgs;
        staticArgs.reserve(totalMeshInstances);
        skinnedArgs.reserve(totalMeshInstances);
        fillIndirectCommands(scene, staticArgs, skinnedArgs);

        const uint64_t instanceBytes = std::max<uint64_t>(instancesData.size() * sizeof(InstanceData), sizeof(InstanceData));

        for (uint32_t i = 0; i < RenderKernel::FramesInFlight; ++i)
        {
            instanceSBs[i] = std::make_unique<vhl::StorageBuffer>(device, allocator, instanceBytes);
            instanceTBs[i] = std::make_unique<vhl::StagingBuffer>(device, allocator, instanceBytes);
            objectSets[i] = descriptorPool.emplace(layouts[1]);
            tracker.emplace(*instanceTBs[i]);
            
            if (!instancesData.empty())
            {
                cpyContext.upload(*instanceSBs[i], instancesData.data(), instanceBytes);
            }
            else tracker.emplace(*instanceSBs[i]);

            vhl::DescriptorWriter writer(objectSets[i]);
            writer.storageBuffer(*instanceSBs[i], 0, 0, instanceSBs[i]->size())
                .storageBuffer(*materialSB, 1, 0, materialSB->size())
                .apply(device);
        }

        std::vector<vhl::DrawIndexedIndirectCommand> args;
        args.reserve(staticArgs.size() + skinnedArgs.size());
        args.insert(args.end(), staticArgs.begin(), staticArgs.end());
        args.insert(args.end(), skinnedArgs.begin(), skinnedArgs.end());

        staticDrawCount = static_cast<uint32_t>(staticArgs.size());
        skinnedDrawCount = static_cast<uint32_t>(skinnedArgs.size());

        const uint64_t indirectBytes = std::max<uint64_t>(args.size() * sizeof(vhl::DrawIndexedIndirectCommand), sizeof(vhl::DrawIndexedIndirectCommand));

        indirectBuf = std::make_unique<vhl::IndirectBuffer>(device, allocator, indirectBytes);

        if (!args.empty())
        {
            cpyContext.upload(*indirectBuf, args.data(), args.size() * sizeof(vhl::DrawIndexedIndirectCommand));
        }
        else 
        {
            tracker.emplace(*indirectBuf);
        }

        cpyContext.flush();
    }

    uint8_t* RenderScene::Impl::build(const core::Scene& scene)
    {
        fillInstances(scene, instancesData);

        if (instancesData.empty())
            return nullptr;

        return reinterpret_cast<uint8_t*>(instancesData.data());
    }
   

    void RenderScene::Impl::fillInstances(const core::Scene& scene, std::vector<InstanceData>& out) const
    {
        out.clear();
        const auto& instances = scene.instances();

        for (uint32_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];
            const auto& model = models.model(inst.modelID);
            const auto& base = modelBases[inst.modelID];
            const uint32_t boneOffset = animationResources->boneOffset(i);

            for (const auto& mesh : model.meshes)
            {
                InstanceData id{};
                id.modelMatrix = inst.transform;
                id.materialIndex = model.materials.empty() ? UINT32_MAX : mesh.materialIndex + base.materialBase;
                id.boneOffset = mesh.hasSkin ? boneOffset : UINT32_MAX;
                out.push_back(id);
            }
        }
    }

    void RenderScene::Impl::fillIndirectCommands(const core::Scene& scene, std::vector<vhl::DrawIndexedIndirectCommand>& outStatic, std::vector<vhl::DrawIndexedIndirectCommand>& outSkinned) const
    {
        auto& instances = scene.instances();
        uint32_t row = 0;
        for (uint32_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];
            const auto& model = models.model(inst.modelID);
            const auto& base = modelBases[inst.modelID];

            for (const auto& mesh : model.meshes)
            {
                vhl::DrawIndexedIndirectCommand arg{};
                arg.indexCount = mesh.indexCount;
                arg.instanceCount = 1;
                arg.firstIndex = mesh.firstIndex + base.indexBase;
                arg.vertexOffset = static_cast<int32_t>(mesh.vertexIndex + (mesh.hasSkin ? base.skinnedBase : base.staticBase));
                arg.firstInstance = row;

                (mesh.hasSkin ? outSkinned : outStatic).push_back(arg);
                ++row;
            }
        }
    }

    RenderScene::~RenderScene() = default;

    const core::Scene& RenderScene::scene() const
    {
        return m_scene;
    }

    core::Scene& RenderScene::scene()
    {
        return m_scene;
    }

    void RenderScene::commit(const core::Scene& scene)
    {
        m_impl->commit(scene);
    }

    vhl::Buffer& RenderScene::staticVB() const
    {
        if (!m_impl->staticVB) throw std::runtime_error("[RenderScene::staticVB] no static vertex buffer (no static meshes in scene). Did you commit the scene before using it?");
        return *m_impl->staticVB;
    }

    vhl::Buffer& RenderScene::skinnedVB() const
    {
        if (!m_impl->skinnedVB) throw std::runtime_error("[RenderScene::skinnedVB] no skinned vertex buffer (no skinned meshes in scene). Did you commit the scene before using it?");
        return *m_impl->skinnedVB;
    }

    uint32_t RenderScene::staticDrawCount() const
    {
        return m_impl->staticDrawCount;
    }

    uint32_t RenderScene::skinnedDrawCount() const
    {
        return m_impl->skinnedDrawCount;
    }

    vhl::Buffer& RenderScene::indexBuf() const
    {
        if (!m_impl->indexBuf) throw std::runtime_error("[RenderScene::indexBuf] not initialized (no meshes in scene). Did you commit the scene before using it?");
        return *m_impl->indexBuf;
    }

    const vhl::DescriptorSetHandle& RenderScene::objectSets(uint32_t slot) const
    {
        if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[RenderScene::objectSets] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
        if (m_impl->objectSets[slot].value == 0) throw std::runtime_error("[RenderScene::objectSets] slot " + std::to_string(slot) + " not committed");
        return m_impl->objectSets[slot];
    }

    vhl::Buffer& RenderScene::indirectBuf() const
    {
        if (!m_impl->indirectBuf) throw std::runtime_error("[RenderScene::indirectBuf] Not initialized. Did you commit the scene before using it?");
        return *m_impl->indirectBuf;
    }

    vhl::Buffer& RenderScene::materialSB() const
    {
        if (!m_impl->materialSB) throw std::runtime_error("[RenderScene::materialSB] no material buffer (no materials in scene)");
        return *m_impl->materialSB;
    }

    vhl::Buffer& RenderScene::instanceSBs(uint32_t slot) const
    {
        if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[RenderScene::instanceSBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
        if (!m_impl->instanceSBs[slot]) throw std::runtime_error("[RenderScene::instanceSBs] slot " + std::to_string(slot) + " not committed");
        return *m_impl->instanceSBs[slot];
    }

    vhl::Buffer& RenderScene::instanceTBs(uint32_t slot) const
    {
        if (slot >= RenderKernel::FramesInFlight) throw std::out_of_range("[RenderScene::instanceTBs] slot out of range (" + std::to_string(slot) + " >= " + std::to_string(RenderKernel::FramesInFlight) + ")");
        if (!m_impl->instanceTBs[slot]) throw std::runtime_error("[RenderScene::instanceTBs] slot " + std::to_string(slot) + " not committed");
        return *m_impl->instanceTBs[slot];
    }

  

    TextureRegistry& RenderScene::textures() const
    {
        return *m_impl->textures;
    }

    uint8_t* RenderScene::build(const core::Scene& scene) const
    {
        if (!m_impl->animationResources) throw std::runtime_error("[RenderScene::build] Not initialized. Did you commit the scene before using it?");
        return m_impl->build(scene);
    }

    uint8_t* RenderScene::buildPlayback(const core::Scene& scene) const
    {
        if (!m_impl->animationResources) throw std::runtime_error("[RenderScene::buildPlayback] Not initialized. Did you commit the scene before using it?");
        return m_impl->animationResources->buildPlayback(scene, m_impl->models);
    }

    const AnimationResources& RenderScene::animations() const
    {
        if (!m_impl->animationResources) throw std::runtime_error("[RenderScene::animations] Not initialized. Did you commit the scene before using it?");
        return *m_impl->animationResources;
    }

    const ShadowResources& RenderScene::shadows() const
    {
        if (!m_impl->shadowResources) throw std::runtime_error("[RenderScene::shadows] Not initialized. Did you commit the scene before using it?");
        return *m_impl->shadowResources;
    }

    math::vec3 RenderScene::sceneMin() const
    {
        return m_impl->sceneMin;
    }

    math::vec3 RenderScene::sceneMax() const
    {
        return m_impl->sceneMax;
    }

    RenderScene::RenderScene(RenderKernel& kernel, core::ModelRegistry& models, const core::Scene& scene)
        : m_impl(new Impl{ kernel, models }), m_scene(scene)
    {
        commit(scene);
    }

}
