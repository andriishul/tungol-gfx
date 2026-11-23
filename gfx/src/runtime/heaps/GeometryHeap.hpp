#pragma once
#include "Pool.hpp"

namespace tungol::runtime
{
    struct GeometryHeapSizes
    {
        uint64_t staticVBSize;
        uint64_t skinnedVBSize;
        uint64_t indexBufSize;
    };

    struct GeometrySizes
    {
        uint64_t staticVBSize;
        uint64_t skinnedVBSize;
        uint64_t indexBufSize;
    };

    struct GeometryAllocation
    {
        std::optional<Range> staticVB, skinnedVB, index;

        bool valid(bool skinned) const
        {
            return skinned ? (skinnedVB && index && !staticVB)
                : (staticVB && index && !skinnedVB);
        }
    };

    class TUNGOL_EXPORT GeometryHeap
    {
    public:
        GeometryHeap(RenderKernel& kernel, const GeometryHeapSizes& sizes);
        ~GeometryHeap();

        GeometryAllocation allocate(const GeometrySizes& sizes);
        void free(const GeometryAllocation& a);

        const Pool& staticPool() const;
        const Pool& skinnedPool() const;
        const Pool& indexedPool() const;

        GeometryHeapSizes sizes() const;
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}