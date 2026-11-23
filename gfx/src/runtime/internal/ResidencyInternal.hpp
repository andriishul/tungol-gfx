#pragma once
#include "ResidencyManager.hpp"

namespace tungol::runtime
{
    struct TUNGOL_EXPORT ResidencyInternal
    {
        static void releaseRetired(ResidencyManager&, uint32_t slot);
        static UploadBatch collectUploads(ResidencyManager&, uint32_t slot);
        static void resetStaging(ResidencyManager&, uint32_t slot);

        static TextureList& textures(ResidencyManager&);
        static GeometryHeap& geomHeap(ResidencyManager&);
        static AnimationHeap& animHeap(ResidencyManager&);
        static MaterialHeap& matHeap(ResidencyManager&);
        static DrawHeap& drawHeap(ResidencyManager&);
    };
}