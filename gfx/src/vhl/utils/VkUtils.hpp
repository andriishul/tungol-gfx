#pragma once

#include <vhl/core/GraphicsCommon.hpp>

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace tungol::vhl::utils
{
    inline VkCullModeFlags toVkCullMode(CullMode m)
    {
        switch (m)
        {
            case CullMode::None:  return VK_CULL_MODE_NONE;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        }
        throw std::runtime_error("[toVkCullMode] Specified cullmode not found!");
    }

    inline VkFrontFace toVkFrontFace(FrontFace f)
    {
        switch (f)
        {
            case FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
            case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        throw std::runtime_error("[toVkFrontFace] Specified front face not found!");
    }
    inline VkBorderColor toVkBorderColor(BorderColor c)
    {
        switch (c)
        {
            case BorderColor::FloatTransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
            case BorderColor::FloatOpaqueBlack: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            case BorderColor::FloatOpaqueWhite: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            case BorderColor::IntTransparentBlack: return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
            case BorderColor::IntOpaqueBlack: return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            case BorderColor::IntOpaqueWhite: return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        }
        
        throw std::runtime_error("[toVkBorderColor] Specified border color not found!");
    }

    inline VkSamplerAddressMode toVkSamplerAddressMode(AddressMode mode) 
    {
        switch (mode)
        {
            case AddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case AddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case AddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case AddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            default: throw std::runtime_error("[toVkSamplerAddressMode] Specified address mode not found!");
        }
    }

    inline VkSamplerMipmapMode toVkSamplerMipmapMode(MipmapMode mode) 
    {
        switch (mode)
        {
            case MipmapMode::Nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case MipmapMode::Linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            default:
                throw std::runtime_error("[toVkSamplerMipmapMode] Specified mip mode not found!");
        }
    }

    inline VkFilter toVkFilter(Filter filter) 
    {
        switch (filter)
        {
        case Filter::Nearest:
            return VK_FILTER_NEAREST;
        case Filter::Linear:
            return VK_FILTER_LINEAR;
        default:
            throw std::runtime_error("[toVkFilter] Specified filter not found!");
        }
    }

    inline VkImageViewType toVkImageViewType(ImageViewType viewType)
    {
        switch (viewType)
        {
            case ImageViewType::OneD:        return VK_IMAGE_VIEW_TYPE_1D;
            case ImageViewType::TwoD:        return VK_IMAGE_VIEW_TYPE_2D;
            case ImageViewType::ThreeD:      return VK_IMAGE_VIEW_TYPE_3D;
            case ImageViewType::Cube:        return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageViewType::OneDArray:   return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageViewType::TwoDArray:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ImageViewType::CubeArray:   return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default:
                throw std::runtime_error("[toVkImageViewType] Specified image view type not found!");
        }
    }

    inline VkImageType toVkImageType(ImageType type)
    {
        switch (type)
        {
            case ImageType::OneD:   return VK_IMAGE_TYPE_1D;
            case ImageType::TwoD:   return VK_IMAGE_TYPE_2D;
            case ImageType::ThreeD: return VK_IMAGE_TYPE_3D;
            default:
                throw std::runtime_error("[toVkImageType] Specified image type not found!");
        }
    }

    inline VkMemoryPropertyFlags toVkMemoryPropertyFlags(MemoryType memtype)
    {
        switch (memtype)
        {
            case MemoryType::GpuOnly:  return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            case MemoryType::CpuToGpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            case MemoryType::GpuToCpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            default: throw std::runtime_error("[toVkMemoryPropertyFlags] memoryType not implented");
        }
    }

    inline VkBufferUsageFlags toVkBufferUsageFlags(BufferType type)
    {
        switch (type)
        {
            case BufferType::Vertex:   return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            case BufferType::Index:    return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            case BufferType::Uniform:  return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            case BufferType::Storage:  return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            case BufferType::Staging:  return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            case BufferType::Indirect: return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            case BufferType::Readback: return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            default:
                throw std::runtime_error("[toVkBufferUsageFlags] No buffer type implemented!");
        }
    }

    inline VkIndexType toVkIndexType(IndexType type)
    {
        switch (type)
        {
            case IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
            case IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
            case IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
            default:
                throw std::runtime_error("[toVkIndexType] No index type supported!");
        }
    }

    inline const std::string ImageUsageToString(ImageUsageFlags fmt)
    {
        std::string imageUsage = "None";

        if ((fmt & ImageUsageFlags::TransferSrc) != ImageUsageFlags::None) 
        {
            imageUsage = "TransferSrc";
        }

        if ((fmt & ImageUsageFlags::TransferDst) != ImageUsageFlags::None)
        {
            if(imageUsage != "None") imageUsage += "  | TransferDst";
            else imageUsage = "TransferDst";
        }

        if ((fmt & ImageUsageFlags::Sampled) != ImageUsageFlags::None)
        {
            if (imageUsage != "None") imageUsage += " | Sampled";
            else imageUsage = "Sampled";
        }

        if ((fmt & ImageUsageFlags::Storage) != ImageUsageFlags::None)
        {
            if (imageUsage != "None") imageUsage += " | Storage";
            else imageUsage = "Storage";
        }

        if ((fmt & ImageUsageFlags::Color) != ImageUsageFlags::None)
        {
            if (imageUsage != "None") imageUsage += " | Color";
            else imageUsage = "Color";
        }

        if ((fmt & ImageUsageFlags::DepthStencil) != ImageUsageFlags::None)
        {
            if (imageUsage != "None") imageUsage += " | DepthStencil";
            else imageUsage = "DepthStencil";
        }

        return imageUsage;
    }

    inline VkImageUsageFlags toVkImageUsageFlags(ImageUsageFlags flags)
    {
        VkImageUsageFlags vkFlags = 0;

        if ((flags & ImageUsageFlags::TransferSrc) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if ((flags & ImageUsageFlags::TransferDst) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if ((flags & ImageUsageFlags::Sampled) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if ((flags & ImageUsageFlags::Storage) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

        if ((flags & ImageUsageFlags::Color) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if ((flags & ImageUsageFlags::DepthStencil) != ImageUsageFlags::None)
            vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        return vkFlags;
    }

    inline VkAttachmentLoadOp toVkAttachmentLoadOp(AttachmentLoadOp loadOp)
    {
        switch (loadOp)
        {
            case AttachmentLoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
            case AttachmentLoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            default: throw std::runtime_error("Unspecified loadOp set!");
        }
    }

    inline VkAttachmentStoreOp toVkAttachmentStoreOp(AttachmentStoreOp storeOp)
    {
        switch (storeOp)
        {
            case AttachmentStoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
            case AttachmentStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            default: throw std::runtime_error("Unspecified storeOp set!");
        }
    }

    inline VkAccessFlags2 toVkAccessFlags2(AccessFlags access)
    {
        VkAccessFlags2 vkAccess = VK_ACCESS_2_NONE;

        if ((access & AccessFlags::IndirectCommandRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        if ((access & AccessFlags::IndexRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_INDEX_READ_BIT;
        if ((access & AccessFlags::VertexAttributeRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        if ((access & AccessFlags::UniformRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_UNIFORM_READ_BIT;
        if ((access & AccessFlags::ShaderRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_SHADER_READ_BIT;
        if ((access & AccessFlags::ShaderWrite) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_SHADER_WRITE_BIT;
        if ((access & AccessFlags::ColorAttachmentRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        if ((access & AccessFlags::ColorAttachmentWrite) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        if ((access & AccessFlags::DepthStencilRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if ((access & AccessFlags::DepthStencilWrite) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if ((access & AccessFlags::TransferRead) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_TRANSFER_READ_BIT;
        if ((access & AccessFlags::TransferWrite) != AccessFlags::None)
            vkAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;

        return vkAccess;
    }

    inline VkImageAspectFlags toVkImageAspectFlags(ImageAspectFlags aspect)
    {
        VkImageAspectFlags vkAspectFlags = VK_IMAGE_ASPECT_NONE;
        if ((aspect & ImageAspectFlags::Color) != ImageAspectFlags::None)
            vkAspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
        if ((aspect & ImageAspectFlags::Depth) != ImageAspectFlags::None)
            vkAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if ((aspect & ImageAspectFlags::Stencil) != ImageAspectFlags::None)
            vkAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

        return vkAspectFlags;
    }

    inline VkPipelineStageFlags2 toVkPipelineStageFlags2(PipelineStageFlags flags)
    {
        VkPipelineStageFlags2 vkFlags = VK_PIPELINE_STAGE_2_NONE;

        if ((flags & PipelineStageFlags::DrawIndirect) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        if ((flags & PipelineStageFlags::IndexInput) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
        if ((flags & PipelineStageFlags::VertexAttributeInput) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
        if ((flags & PipelineStageFlags::VertexShader) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        if ((flags & PipelineStageFlags::FragmentShader) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        if ((flags & PipelineStageFlags::ComputeShader) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        if ((flags & PipelineStageFlags::EarlyFragmentTests) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        if ((flags & PipelineStageFlags::LateFragmentTests) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        if ((flags & PipelineStageFlags::ColorAttachmentOutput) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        if ((flags & PipelineStageFlags::Transfer) != PipelineStageFlags::None)
            vkFlags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        
        return vkFlags;
    }

    inline VkImageLayout toVkImageLayout(ImageLayout layout)
    {
        switch (layout)
        {
            case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
            case ImageLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ImageLayout::DepthStencilAttachment:return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageLayout::ShaderReadOnly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ImageLayout::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ImageLayout::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ImageLayout::PresentSrc: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default: throw std::runtime_error("[toVkImageLayout] Unknown image layout specified!");
        }
    }

    inline SampleCount toSampleCount(VkSampleCountFlags samples)
    {
        uint32_t sampleCount = 0;

        if ((samples & VK_SAMPLE_COUNT_1_BIT) != 0)
            sampleCount |= (uint32_t) SampleCount::One;
        if ((samples & VK_SAMPLE_COUNT_2_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::Two;
        if ((samples & VK_SAMPLE_COUNT_4_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::Four;
        if ((samples & VK_SAMPLE_COUNT_8_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::Eight;
        if ((samples & VK_SAMPLE_COUNT_16_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::Sixteen;
        if ((samples & VK_SAMPLE_COUNT_32_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::ThirtyTwo;
        if ((samples & VK_SAMPLE_COUNT_64_BIT) != 0)
            sampleCount |= (uint32_t)SampleCount::SixtyFour;

        return sampleCount != 0 ? static_cast<SampleCount>(sampleCount) : SampleCount::One;
    }

    inline VkSampleCountFlagBits toVkSampleCountFlagBits(SampleCount samples)
    {
        return static_cast<VkSampleCountFlagBits>(samples);
    }

    inline VkStencilOp toVkStencilOp(StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Keep: return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero: return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace: return VK_STENCIL_OP_REPLACE;
        case StencilOp::Invert: return VK_STENCIL_OP_INVERT;
        case StencilOp::IncrementAndClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecrementAndClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        default:
            throw std::runtime_error("[toVkStencilOp] Unsupported stencil op!");
        }
    }

    inline VkCompareOp toVkCompareOp(CompareOp op)
    {
        switch (op)
        {
            case CompareOp::Never: return VK_COMPARE_OP_NEVER;
            case CompareOp::Less: return VK_COMPARE_OP_LESS;
            case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
            case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
            case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
            default: throw std::runtime_error("[toVkCompareOp] Unsupported compare op!");
        }
    }

    inline VkViewport toVkViewport(Viewport viewport)
    {
        return
        {
            .x = viewport.x,
            .y = viewport.y,
            .width = viewport.width,
            .height = viewport.height,
            .minDepth = viewport.minDepth,
            .maxDepth = viewport.maxDepth
        };
    }

    inline VkRect2D toVkScissor(Scissor scissor)
    {
        return
        {
            .offset = VkOffset2D{ scissor.offsetX, scissor.offsetY },
            .extent = VkExtent2D { scissor.width, scissor.height }
        };
    }

    inline VkPolygonMode toVkPolygonMode(PolygonMode mode)
    {
        switch (mode)
        {
            case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
            case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
            default: throw std::runtime_error("[toVkPolygonMode] Current polygon mode not found!");
        }
    }

    inline VkPrimitiveTopology toVkPrimitiveTopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
            case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case PrimitiveTopology::PatchList: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        }
        throw std::runtime_error("[toVkPrimitiveTopology] Current topology not found!");
    }

    inline const std::string FormatToString(Format fmt)
    {
        switch (fmt)
        {
            case Format::Undefined: return "Undefined";
            case Format::R8Unorm: return "R8Unorm";
            case Format::R8Snorm: return "R8Snorm";
            case Format::R8Uint: return "R8Uint";
            case Format::R8Sint: return "R8Sint";
            case Format::R8Srgb: return "R8Srgb";
            case Format::Rg8Unorm: return "Rg8Unorm";
            case Format::Rg8Snorm: return "Rg8Snorm";
            case Format::Rg8Uint: return "Rg8Uint";
            case Format::Rg8Sint: return "Rg8Sint";
            case Format::Rg8Srgb: return "Rg8Srgb";
            case Format::Rgb8Unorm: return "Rgb8Unorm";
            case Format::Rgb8Snorm: return "Rgb8Snorm";
            case Format::Rgb8Uint: return "Rgb8Uint";
            case Format::Rgb8Sint: return "Rgb8Sint";
            case Format::Rgb8Srgb: return "Rgb8Srgb";
            case Format::Rgba8Unorm: return "Rgba8Unorm";
            case Format::Rgba8Snorm: return "Rgba8Snorm";
            case Format::Rgba8Uint: return "Rgba8Uint";
            case Format::Rgba8Sint: return "Rgba8Sint";
            case Format::Rgba8Srgb: return "Rgba8Srgb";
            case Format::Bgra8Unorm: return "Bgra8Unorm";
            case Format::Bgra8Srgb: return "Bgra8Srgb";
            case Format::R16Uint: return "R16Uint";
            case Format::R16Sint: return "R16Sint";
            case Format::R16Float: return "R16Float";
            case Format::Rg16Float: return "Rg16Float";
            case Format::Rgba16Float: return "Rgba16Float";
            case Format::R32Uint: return "R32Uint";
            case Format::R32Sint: return "R32Sint";
            case Format::R32Float: return "R32Float";
            case Format::Rg32Float: return "Rg32Float";
            case Format::Rgb32Float: return "Rgb32Float";
            case Format::Rgba32Float: return "Rgba32Float";
            case Format::Rgb10A2Unorm: return "Rgb10A2Unorm";
            case Format::Rg11B10Float: return "Rg11B10Float";
            case Format::D16Unorm: return "D16Unorm";
            case Format::D24UnormS8Uint: return "D24UnormS8Uint";
            case Format::D32Float: return "D32Float";
            case Format::D32FloatS8Uint: return "D32FloatS8Uint";
            case Format::Rgba32Sint: return "Rgba32Sint";
            case Format::Rgba32Uint: return "Rgba32Uint";
            default: throw std::runtime_error("[FormatToString] Unsupported Format!");
        }
    }

    inline VkFormat toVkFormat(Format format)
    {
        switch (format)
        {
            case Format::Undefined: return VK_FORMAT_UNDEFINED;

            case Format::R8Unorm: return VK_FORMAT_R8_UNORM;
            case Format::R8Snorm: return VK_FORMAT_R8_SNORM;
            case Format::R8Uint: return VK_FORMAT_R8_UINT;
            case Format::R8Sint: return VK_FORMAT_R8_SINT;
            case Format::R8Srgb: return VK_FORMAT_R8_SRGB;

            case Format::Rg8Unorm: return VK_FORMAT_R8G8_UNORM;
            case Format::Rg8Snorm: return VK_FORMAT_R8G8_SNORM;
            case Format::Rg8Uint: return VK_FORMAT_R8G8_UINT;
            case Format::Rg8Sint: return VK_FORMAT_R8G8_SINT;
            case Format::Rg8Srgb: return VK_FORMAT_R8G8_SRGB;

            case Format::Rgb8Unorm: return VK_FORMAT_R8G8B8_UNORM;
            case Format::Rgb8Snorm: return VK_FORMAT_R8G8B8_SNORM;
            case Format::Rgb8Uint: return VK_FORMAT_R8G8B8_UINT;
            case Format::Rgb8Sint: return VK_FORMAT_R8G8B8_SINT;
            case Format::Rgb8Srgb: return VK_FORMAT_R8G8B8_SRGB;

            case Format::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case Format::Rgba8Snorm: return VK_FORMAT_R8G8B8A8_SNORM;
            case Format::Rgba8Uint: return VK_FORMAT_R8G8B8A8_UINT;
            case Format::Rgba8Sint: return VK_FORMAT_R8G8B8A8_SINT;
            case Format::Rgba8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;

            case Format::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case Format::Bgra8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;

            case Format::R16Uint: return VK_FORMAT_R16_UINT;
            case Format::R16Sint: return VK_FORMAT_R16_SINT;
            case Format::R16Float: return VK_FORMAT_R16_SFLOAT;
            case Format::Rg16Float: return VK_FORMAT_R16G16_SFLOAT;
            case Format::Rgba16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;

            case Format::R32Uint: return VK_FORMAT_R32_UINT;
            case Format::R32Sint: return VK_FORMAT_R32_SINT;
            case Format::R32Float: return VK_FORMAT_R32_SFLOAT;
            case Format::Rg32Float: return VK_FORMAT_R32G32_SFLOAT;
            case Format::Rgb32Float: return VK_FORMAT_R32G32B32_SFLOAT;
            case Format::Rgba32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;

            case Format::Rgba32Sint: return VK_FORMAT_R32G32B32A32_SINT;
            case Format::Rgba32Uint: return VK_FORMAT_R32G32B32A32_UINT;

            case Format::Rgb10A2Unorm: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
            case Format::Rg11B10Float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

            case Format::D16Unorm: return VK_FORMAT_D16_UNORM;
            case Format::D24UnormS8Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
            case Format::D32Float: return VK_FORMAT_D32_SFLOAT;
            case Format::D32FloatS8Uint: return VK_FORMAT_D32_SFLOAT_S8_UINT;

            default: throw std::runtime_error("[toVkFormat] Unsupported format!");
        }
    }

    inline Format toFormat(VkFormat vk_format)
    {
        switch (vk_format)
        {
        case VK_FORMAT_UNDEFINED:                       return Format::Undefined;
        case VK_FORMAT_R8_UNORM:                        return Format::R8Unorm;
        case VK_FORMAT_R8_SNORM:                        return Format::R8Snorm;
        case VK_FORMAT_R8_UINT:                         return Format::R8Uint;
        case VK_FORMAT_R8_SINT:                         return Format::R8Sint;
        case VK_FORMAT_R8G8_UNORM:                      return Format::Rg8Unorm;
        case VK_FORMAT_R8G8_SNORM:                      return Format::Rg8Snorm;
        case VK_FORMAT_R8G8_UINT:                       return Format::Rg8Uint;
        case VK_FORMAT_R8G8_SINT:                       return Format::Rg8Sint;
        case VK_FORMAT_R8G8B8_UNORM:                    return Format::Rgb8Unorm;
        case VK_FORMAT_R8G8B8_SRGB:                     return Format::Rgb8Srgb;
        case VK_FORMAT_R8G8B8A8_UNORM:                  return Format::Rgba8Unorm;
        case VK_FORMAT_R8G8B8A8_SRGB:                   return Format::Rgba8Srgb;
        case VK_FORMAT_B8G8R8A8_UNORM:                  return Format::Bgra8Unorm;
        case VK_FORMAT_B8G8R8A8_SRGB:                   return Format::Bgra8Srgb;
        case VK_FORMAT_R16_UINT:                        return Format::R16Uint;
        case VK_FORMAT_R16_SINT:                        return Format::R16Sint;
        case VK_FORMAT_R16_SFLOAT:                      return Format::R16Float;
        case VK_FORMAT_R16G16_SFLOAT:                   return Format::Rg16Float;
        case VK_FORMAT_R16G16B16A16_SFLOAT:             return Format::Rgba16Float;
        case VK_FORMAT_R32_UINT:                        return Format::R32Uint;
        case VK_FORMAT_R32_SINT:                        return Format::R32Sint;
        case VK_FORMAT_R32_SFLOAT:                      return Format::R32Float;
        case VK_FORMAT_R32G32_SFLOAT:                   return Format::Rg32Float;
        case VK_FORMAT_R32G32B32_SFLOAT:                return Format::Rgb32Float;
        case VK_FORMAT_R32G32B32A32_SFLOAT:             return Format::Rgba32Float;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:        return Format::Rgb10A2Unorm;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:         return Format::Rg11B10Float;
        case VK_FORMAT_D16_UNORM:                       return Format::D16Unorm;
        case VK_FORMAT_D24_UNORM_S8_UINT:               return Format::D24UnormS8Uint;
        case VK_FORMAT_D32_SFLOAT:                      return Format::D32Float;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:              return Format::D32FloatS8Uint;
        case VK_FORMAT_R32G32B32A32_SINT :              return Format::Rgba32Sint;
        case VK_FORMAT_R32G32B32A32_UINT :              return Format::Rgba32Uint;
        default:
            throw std::runtime_error("[toFormat] Unsupported VkFormat!");
        }
    }

    inline VkShaderStageFlags toVkShaderStageFlags(ShaderStageFlags stage)
    {
        VkShaderStageFlags vkShaderStage = 0;

        if ((stage & ShaderStageFlags::Vertex) != ShaderStageFlags::None)
            vkShaderStage |= VK_SHADER_STAGE_VERTEX_BIT;
        if ((stage & ShaderStageFlags::Fragment) != ShaderStageFlags::None)
            vkShaderStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if ((stage & ShaderStageFlags::Compute) != ShaderStageFlags::None)
            vkShaderStage |= VK_SHADER_STAGE_COMPUTE_BIT;

        return vkShaderStage;
    }

    inline VkPushConstantRange toVkPushConstantRange(PushConstantRange range)
    {
        return VkPushConstantRange{ toVkShaderStageFlags(range.stage), range.offset, range.size };
    }

    inline VkDescriptorType toVkDescriptorType(DescriptorType type)
    {
        switch (type)
        {
            case DescriptorType::SampledImage: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case DescriptorType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case DescriptorType::UniformBufferDynamic: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case DescriptorType::StorageBufferDynamic: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }
        throw std::runtime_error("[toVkDescriptorType] Unknown DescriptorType");
    }

    inline const char* VkResultToString(VkResult vk_result)
    {
        switch (vk_result)
        {
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    #if VK_HEADER_VERSION >= 135
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    #endif
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            default: return "UNKNOWN_VK_RESULT";
        }
    }

    inline void VkCheckImpl(VkResult vk_result, const char* expression, const char* file, int line)
    {
        if (vk_result != VK_SUCCESS && vk_result != VK_SUBOPTIMAL_KHR && vk_result != VK_ERROR_OUT_OF_DATE_KHR && vk_result != VK_NOT_READY)
        {
            std::ostringstream oss;
            oss << expression << " failed at " << std::quoted(file)
                << " line: " << line << " (" << VkResultToString(vk_result) << ")";
            throw std::runtime_error(oss.str());
        }
    }
}

#define VK_CHECK(x) ::tungol::vhl::utils::VkCheckImpl(x, #x, __FILE__, __LINE__)