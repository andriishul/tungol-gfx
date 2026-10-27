#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "VkUtils.hpp"

namespace tungol::vhl
{
    struct PhysicalDeviceDesc
    {
        std::string name = "";

        VkPhysicalDevice physical = nullptr;

        VkPhysicalDeviceType type;

        VkPhysicalDeviceLimits limits;

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceVulkan12Properties properties12;
        VkPhysicalDeviceVulkan13Properties properties13;

        VkPhysicalDeviceFeatures features;

        VkPhysicalDeviceVulkan12Features features12;
        VkPhysicalDeviceVulkan13Features features13;
    };

    struct DeviceDesc
    {
        PhysicalDeviceDesc description;

        std::vector<QueueFamilyDetails> queueFamilies;

        uint32_t enabledExtensionCount = 0;
        const char* const* ppEnabledExtensionNames = nullptr;
    };


    struct SurfaceDesc
    {
        VkSurfaceKHR surface;

        VkSurfaceCapabilitiesKHR capabilities;

        std::vector<VkPresentModeKHR> presentationModes;

        std::vector<VkSurfaceFormatKHR> formats;
    };

    
    struct MemoryRequirements
    {
        uint64_t alignment, size;

        MemoryType memoryType;
        uint32_t memoryTypeIndex;
    };


    class Enumerator
    {
    public:
        class PhysicalDevices
        {
        public:
            /*
                Enumerates all available Vulkan physical devices and caches their
                properties, limits, and supported features for later use.
            */
            static void init(const VkInstance& instance);

            static const PhysicalDeviceDesc& get(const std::string& device_name);
            static const PhysicalDeviceDesc& get(uint32_t index);
            // gets first possible device of specified device type
            static const PhysicalDeviceDesc& get(VkPhysicalDeviceType type);

            static inline uint32_t size() { return static_cast<uint32_t>(s_physicalDevicesDesc.size()); }
        private:
            static inline std::vector<PhysicalDeviceDesc> s_physicalDevicesDesc;
        };

        class Extensions
        {
        public:
            static std::vector<std::string> get_instance();
            static std::vector<std::string> get_device(const VkPhysicalDevice& physical);
        };

        class Layers
        {
        public:
            static std::vector<std::string> get_instance();
        };

        class Swapchain 
        {
        public:
            static SurfaceDesc get(const VkPhysicalDevice& physical, const VkSurfaceKHR& surface);
        };

        class QueueFamilyIndices
        {
        public:
            static std::vector<QueueFamilyDetails> list(const VkPhysicalDevice& physical, const VkSurfaceKHR& surface);
        };

        class MemoryFilter
        {
        private:
            static VkPhysicalDeviceMemoryProperties get_physical_device_properties(const VkPhysicalDevice& physical);
        public:
            static MemoryRequirements get_image_requirements(const VkDevice& logical, const VkPhysicalDevice& physical, const VkImage& image, MemoryType required);
            static MemoryRequirements get_buffer_requirements(const VkDevice& logical, const VkPhysicalDevice& physical, const VkBuffer& buffer, MemoryType required);
        };
    };

    class ShaderModule 
    {
    public:
        static VkShaderModule create(const VkDevice& logical, const uint32_t* code, size_t size);
    };
}
