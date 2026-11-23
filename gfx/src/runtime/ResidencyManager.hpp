#pragma once
#include <core/TungolExport.hpp>
#include <core/Fwd.hpp>

#include <core/Scene.hpp>

#include <runtime/heaps/AnimationHeap.hpp>
#include <runtime/heaps/GeometryHeap.hpp>
#include <runtime/heaps/MaterialHeap.hpp>

namespace tungol::runtime 
{
    enum class ResidencyState : uint8_t
    {
        Absent,       
        Allocating,   
        Resident,     
        Retiring 
    };

    struct ResidentModel
    {
        ResidencyState state = ResidencyState::Absent;
        uint32_t refCount = 0;
        uint32_t boneCount = 0;

        MaterialAllocation materials;
        GeometryAllocation geometry;
        AnimationAllocation animation;
        std::vector<uint32_t> textureSlots;
    };

    struct BufferCopy
    {
        vhl::Buffer* src;
        uint64_t srcOffset;
        
        vhl::Buffer* dst;
        
        uint64_t dstOffset;
        uint64_t size;
    };

    struct ImageCopy
    {
        vhl::Buffer* src;        
        uint64_t srcOffset;
        
        vhl::Image* dst;
        
        uint32_t mip;
        uint32_t layer;

        uint32_t width;
        uint32_t height;
        uint32_t depth;

        uint64_t size;
    };

    struct UploadBatch
    {
        std::vector<BufferCopy> buffers;
        std::vector<ImageCopy> images;
    };


    class TUNGOL_EXPORT ResidencyManager
    {
    public:
        ResidencyManager(RenderKernel& kernel, uint32_t maxTextures);
        ~ResidencyManager();

        void retain(core::ModelID id, const core::ModelRecord& record);
        void release(core::ModelID id);
        
        std::optional<uint32_t> clipIndex(core::ModelID id, std::string_view name) const;

        bool isResident(core::ModelID id) const;
        const ResidentModel* find(core::ModelID id) const;

        uint64_t version() const;
    private:
        friend class ResidencyInternal;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };


}