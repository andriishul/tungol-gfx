#pragma once
#include <cstdint>

namespace tungol::vhl
{
    enum class UsageIntent : uint32_t
    {
        None = 0,

        TransferSrc = 1U << 0,
        TransferDst = 1U << 1,

        ShaderReadVertex = 1U << 2,
        ShaderReadFragment = 1U << 3,
        ShaderReadCompute = 1U << 4,

        ShaderWriteVertex = 1U << 5,
        ShaderWriteFragment = 1U << 6,
        ShaderWriteCompute = 1U << 7,

        ColorAttachment = 1U << 8,
        DepthStencilAttachment = 1U << 9,

        Present = 1U << 10,

        VertexAttributeRead = 1U << 11,
        IndexRead = 1U << 12,
        IndirectRead = 1U << 13,

        UniformReadVertex = 1U << 14,
        UniformReadFragment = 1U << 15,
        UniformReadCompute = 1U << 16,
    };

    enum class BufferType : uint32_t
    {
        Vertex,
        Index,
        Uniform,
        Storage,
        Staging,
        Indirect,
        Readback
    };

    enum class QueueUsage : uint32_t
    {
        None = 0,
        Graphics = 1U << 0,
        Present = 1U << 1,
        Compute = 1U << 2,
        Transfer = 1U << 3,
    };

    enum class ImageViewType : uint32_t
    {
        OneD = 0,
        TwoD = 1,
        ThreeD = 2,
        Cube = 3,
        OneDArray = 4,
        TwoDArray = 5,
        CubeArray = 6,
    };

    enum class ImageType : uint32_t
    {
        OneD,
        TwoD,
        ThreeD
    };

    enum class DescriptorType : uint32_t
    {
        SampledImage,
        StorageImage,
        UniformBuffer,
        UniformBufferDynamic,
        StorageBuffer,
        StorageBufferDynamic,
    };

    enum class ShaderStageFlags : uint32_t
    {
        None = 0,
        Vertex = 1U << 0,
        Fragment = 1U << 1,
        Compute = 1U << 2
    };

    enum class ImageUsageFlags : uint32_t
    {
        None = 0,
        TransferSrc = 1U << 0,
        TransferDst = 1U << 1,
        Sampled = 1U << 2,
        Storage = 1U << 3,
        Color = 1U << 4,
        DepthStencil = 1U << 5
    };

    enum class PipelineStageFlags : uint64_t
    {
        None = 0,
        DrawIndirect = 1ULL << 0,
        IndexInput = 1ULL << 1,
        VertexAttributeInput = 1ULL << 2,
        VertexShader = 1ULL << 3,
        FragmentShader = 1ULL << 4,
        ComputeShader = 1ULL << 5,
        EarlyFragmentTests = 1ULL << 6,
        LateFragmentTests = 1ULL << 7,
        ColorAttachmentOutput = 1ULL << 8,
        Transfer = 1ULL << 9
    };

    enum class ImageAspectFlags : uint32_t
    {
        None = 0,
        Color = 1U << 0,
        Depth = 1U << 1,
        Stencil = 1U << 2
    };

    enum class AccessFlags : uint64_t
    {
        None = 0,
        IndirectCommandRead = 1ULL << 0,
        IndexRead = 1ULL << 1,
        VertexAttributeRead = 1ULL << 2,
        UniformRead = 1ULL << 3,
        ShaderRead = 1ULL << 4,
        ShaderWrite = 1ULL << 5,
        ColorAttachmentRead = 1ULL << 6,
        ColorAttachmentWrite = 1ULL << 7,
        DepthStencilRead = 1ULL << 8,
        DepthStencilWrite = 1ULL << 9,
        TransferRead = 1ULL << 10,
        TransferWrite = 1ULL << 11,
    };


    enum class ImageLayout : uint32_t
    {
        Undefined,
        General,
        ColorAttachment,
        DepthStencilAttachment,
        ShaderReadOnly,
        TransferSrc,
        TransferDst,
        PresentSrc
    };

    enum class AttachmentLoadOp : uint32_t
    {
        Load = 0,
        Clear = 1,
        DontCare = 2
    };

    enum class AttachmentStoreOp : uint32_t
    {
        Store = 0,
        DontCare = 1
    };

    enum class Filter : uint32_t
    {
        Nearest, Linear
    };
    enum class MipmapMode : uint32_t { Nearest, Linear };

    enum class AddressMode : uint32_t
    {
        Repeat, MirroredRepeat,
        ClampToEdge, ClampToBorder,
        MirrorClampToEdge
    };

    enum class BorderColor
    {
        FloatTransparentBlack,
        FloatOpaqueBlack,
        FloatOpaqueWhite,
        IntTransparentBlack,
        IntOpaqueBlack,
        IntOpaqueWhite,
    };

    enum class IndexType : uint32_t
    {
        Uint16,
        Uint32,
        Uint8
    };

    enum class MemoryType : uint32_t
    {
        GpuOnly,    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT

        CpuToGpu,   // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

        GpuToCpu    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    };

   
    struct DescriptorBinding
    {
        uint32_t binding = 0;
        uint32_t count = 0;
        DescriptorType type = DescriptorType::SampledImage;
        ShaderStageFlags stage = ShaderStageFlags::None;

        bool partiallyBound = false;
        bool updateAfterBind = false;   
        bool variableCount = false;
    };

    enum class SampleCount : uint32_t
    {
        One = 1,
        Two = 2,
        Four = 4,
        Eight = 8,
        Sixteen = 16,
        ThirtyTwo = 32,
        SixtyFour = 64
    };

    enum class Format : uint32_t
    {
        Undefined,

        // 8‑bit
        R8Unorm, R8Snorm, R8Uint, R8Sint, R8Srgb,
        Rg8Unorm, Rg8Snorm, Rg8Uint, Rg8Sint, Rg8Srgb,
        Rgb8Unorm, Rgb8Snorm, Rgb8Uint, Rgb8Sint, Rgb8Srgb,
        Rgba8Unorm, Rgba8Snorm, Rgba8Uint, Rgba8Sint, Rgba8Srgb,
        Bgra8Unorm, Bgra8Srgb,

        // 16‑bit
        R16Uint, R16Sint, R16Float,
        Rg16Float,
        Rgba16Float,

        // 32‑bit
        R32Uint, R32Sint, R32Float,
        Rg32Float, Rgb32Float, Rgba32Float,
        Rgba32Uint, Rgba32Sint,

        // Packed
        Rgb10A2Unorm,
        Rg11B10Float,

        // Depth/stencil
        D16Unorm,
        D24UnormS8Uint,
        D32Float,
        D32FloatS8Uint
    };

    enum class PrimitiveTopology
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        PatchList
    };

    enum class PolygonMode
    {
        Fill,
        Line,
        Point
    };

    enum class CullMode 
    {
        None, Front, Back
    };

    enum class FrontFace 
    {
        Clockwise, CounterClockwise
    };


    enum class CompareOp
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class StencilOp
    {
        Keep,
        Zero,
        Replace,
        Invert,
        IncrementAndClamp,
        DecrementAndClamp
    };

    struct PushConstantRange
    {
        ShaderStageFlags stage;
        uint32_t offset, size;
    };

    struct Viewport
    {
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        float minDepth = 0.0f, maxDepth = 1.0f;
    };

    struct Scissor
    {
        int32_t offsetX = 0, offsetY = 0;
        uint32_t width = 0, height = 0;
    };

    using QueueFamily = uint32_t;
    constexpr QueueFamily QueueFamilyIgnored = UINT32_MAX;

    struct QueueFamilyDetails
    {
        QueueFamily family = 0;
        uint32_t count = 0;

        bool graphics = false;
        bool compute = false;
        bool transfer = false;
        bool present = false;
    };

    constexpr uint32_t RemainingMipLevels = UINT32_MAX;
    constexpr uint32_t RemainingArrayLayers = UINT32_MAX;
    constexpr uint32_t BufferWholeSize = UINT32_MAX;

    constexpr inline UsageIntent operator|(UsageIntent a, UsageIntent b)
    {
        return static_cast<UsageIntent>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr inline UsageIntent& operator|=(UsageIntent& a, UsageIntent b)
    {
        a = static_cast<UsageIntent>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
        return a;
    }

    constexpr inline UsageIntent operator&(UsageIntent a, UsageIntent b)
    {
        return static_cast<UsageIntent>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    constexpr inline UsageIntent operator~(UsageIntent a)
    {
        return static_cast<UsageIntent>(~static_cast<uint32_t>(a));
    }

    constexpr inline PipelineStageFlags operator|(PipelineStageFlags a, PipelineStageFlags b)
    {
        return static_cast<PipelineStageFlags>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    constexpr inline PipelineStageFlags operator&(PipelineStageFlags a, PipelineStageFlags b)
    {
        return static_cast<PipelineStageFlags>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
    }

    constexpr inline ImageAspectFlags operator&(ImageAspectFlags a1, ImageAspectFlags a2)
    {
        return static_cast<ImageAspectFlags>(static_cast<uint32_t>(a1) & static_cast<uint32_t>(a2));
    }

    constexpr inline ImageAspectFlags operator|(ImageAspectFlags a1, ImageAspectFlags a2)
    {
        return static_cast<ImageAspectFlags>(static_cast<uint32_t>(a1) | static_cast<uint32_t>(a2));
    }

    inline bool samplesSupported(SampleCount s1, SampleCount s2)
    {
        uint32_t us2 = static_cast<uint32_t>(s2);
        return (static_cast<uint32_t>(s1) & us2) == us2;
    }

    constexpr inline ShaderStageFlags operator|(ShaderStageFlags f1, ShaderStageFlags f2)
    {
        return static_cast<ShaderStageFlags>(static_cast<uint32_t>(f1) | static_cast<uint32_t>(f2));
    }

    constexpr inline ShaderStageFlags operator&(ShaderStageFlags f1, ShaderStageFlags f2)
    {
        return static_cast<ShaderStageFlags>(static_cast<uint32_t>(f1) & static_cast<uint32_t>(f2));
    }

    constexpr inline AccessFlags operator&(AccessFlags a1, AccessFlags a2)
    {
        return static_cast<AccessFlags>(static_cast<uint64_t>(a1) & static_cast<uint64_t>(a2));
    }

    constexpr inline AccessFlags operator|(AccessFlags a1, AccessFlags a2)
    {
        return static_cast<AccessFlags>(static_cast<uint64_t>(a1) | static_cast<uint64_t>(a2));
    }

    constexpr inline QueueUsage operator&(QueueUsage u1, QueueUsage u2)
    {
        return static_cast<QueueUsage>(static_cast<uint32_t>(u1) & static_cast<uint32_t>(u2));
    }

    constexpr inline QueueUsage operator|(QueueUsage u1, QueueUsage u2)
    {
        return static_cast<QueueUsage>(static_cast<uint32_t>(u1) | static_cast<uint32_t>(u2));
    }

    constexpr inline QueueUsage toQueueUsage(const QueueFamilyDetails& details)
    {
        QueueUsage usage = QueueUsage::None;

        if (details.compute)  usage = usage | QueueUsage::Compute;
        if (details.graphics) usage = usage | QueueUsage::Graphics;
        if (details.transfer) usage = usage | QueueUsage::Transfer;
        if (details.present)  usage = usage | QueueUsage::Present;

        return usage;
    }

    constexpr inline ImageUsageFlags operator|(ImageUsageFlags f1, ImageUsageFlags f2)
    {
        return static_cast<ImageUsageFlags>(static_cast<uint32_t>(f1) | static_cast<uint32_t>(f2));
    }

    constexpr inline ImageUsageFlags operator&(ImageUsageFlags f1, ImageUsageFlags f2)
    {
        return static_cast<ImageUsageFlags>(static_cast<uint32_t>(f1) & static_cast<uint32_t>(f2));
    }

    constexpr inline AccessFlags AccessWriteBit = AccessFlags::ShaderWrite | AccessFlags::DepthStencilWrite | AccessFlags::TransferWrite | AccessFlags::ColorAttachmentWrite;
}
