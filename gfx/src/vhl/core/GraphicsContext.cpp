#include "GraphicsContext.hpp"
#include <window/surface/SurfaceProvider.hpp>

#include <vhl/utils/VkInspector.hpp>

#include <event/GraphicsEventArgs.hpp>
#include <event/EventBus.hpp>

#include <iterator>

constexpr const char* k_ValidationLayerName = "VK_LAYER_KHRONOS_validation";

#if defined(_DEBUG)
constexpr bool k_EnableValidationLayers = true;
#else
constexpr bool k_EnableValidationLayers = false;
#endif // DEBUG


constexpr const char* k_InstanceExtensions[] =
{
#if defined(_DEBUG)
    "VK_EXT_debug_utils",
#endif
    "VK_KHR_surface",
#if defined(_WIN32)
    "VK_KHR_win32_surface"
#elif defined(__linux__)
    "VK_KHR_xcb_surface",  
    "VK_KHR_wayland_surface"
#endif
};

constexpr const char* k_DeviceExtensions[] =
{
    "VK_KHR_swapchain"
};

constexpr uint32_t k_InstanceExtensionCount = static_cast<uint32_t>(std::size(k_InstanceExtensions));


constexpr uint32_t k_DeviceExtensionCount = static_cast<uint32_t>(std::size(k_DeviceExtensions));

namespace tungol::vhl
{
    struct ValidationLayerInfo
    {
        void* pUserData = nullptr;
        const char* pEnabledLayerName = nullptr;
    };


    struct InstanceCreationArgs
    {
        const char* rootName = nullptr;

        ValidationLayerInfo validationLayer;

        uint32_t enabledExtensionCount = 0;
        const char* const* ppEnabledExtensionNames = nullptr;
    };



    struct GraphicsContext::Impl 
    {
        VkInstance instance = nullptr;
        VkPhysicalDevice physical = nullptr;
        VkDebugUtilsMessengerEXT debugMessenger = 0;
        VkSurfaceKHR surface = 0;

        std::unique_ptr<Device> device;
        std::unique_ptr<Swapchain> swapchain;
    };

    VKAPI_ATTR VkResult VKAPI_CALL CreateDebugUtilsMessengerEXT(
        VkInstance                                instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pMessenger) 
    {
        if (PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))
        {
            return func(instance, pCreateInfo, pAllocator, pMessenger);
        }

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VKAPI_ATTR void VKAPI_CALL DestroyDebugUtilsMessengerEXT(
        VkInstance                                  instance,
        VkDebugUtilsMessengerEXT                    messenger,
        const VkAllocationCallbacks* pAllocator) 
    {
        if (PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")) 
        {
            func(instance, messenger, pAllocator);
        }
    }


    static VkBool32 VKAPI_PTR VK_LOG(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {

        event::MessageType type = event::MessageType::None;
        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            type = event::MessageType::Verbose;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            type = event::MessageType::Info;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            type = event::MessageType::Warning;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            type = event::MessageType::Error;
            break;
        }

        std::ostringstream oss;
        oss << "\n"
            << "ID NAME     : " << (pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "None") << "\n"
            << "MESSAGE ID  : " << pCallbackData->messageIdNumber << "\n"
            << "MESSAGE : " << (pCallbackData->pMessage ? pCallbackData->pMessage : "None") << "\n";

        event::GraphicsEventArgs graphicsArgs(tungol::event::GraphicsAPI::Vulkan, type, oss.str());
        event::EventBus::instance().fire(tungol::event::EventType::Graphics, graphicsArgs);

        if (type == event::MessageType::Error)
        {
            if (k_EnableValidationLayers) abort();
        }
        return VK_FALSE;
    }

    static VkDebugUtilsMessengerCreateInfoEXT CreateValidationLayerInfoEXT(
        void* pUserData)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | 
            // Remove these flags
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,

            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,

            .pfnUserCallback = &VK_LOG,
            .pUserData = pUserData
        };

        return debugMessengerInfo;
    }

    static void CheckInstance(const char* const* extensions,
        uint32_t count, const char* layer)
    {
        const auto instanceEXTs = Enumerator::Extensions::get_instance();

        for (size_t i = 0; i < count; ++i)
        {
            const char* extension = extensions[i];
            if (std::find_if(instanceEXTs.cbegin(), instanceEXTs.cend(),
                [extension](const std::string& extStr)
                {
                    return extStr == extension;
                }) == instanceEXTs.cend())
            {
                std::ostringstream ss;
                ss << "[CheckInstance] Instance extension: " << std::quoted(extension)
                    << " not found";

                throw std::runtime_error(ss.str());
            }
        }

        auto layers = Enumerator::Layers::get_instance();

        if (std::find(layers.cbegin(), layers.cend(), layer) == layers.cend())
        {
            std::ostringstream ss;
            ss << "[CheckInstance] Layer: " << std::quoted(layer)
                << " not found";

            throw std::runtime_error(ss.str());
        }

    }

    static VkInstance CreateInstance(
        const InstanceCreationArgs& initArgs,
        VkDebugUtilsMessengerEXT& debugMessenger)
    {
        CheckInstance(initArgs.ppEnabledExtensionNames, initArgs.enabledExtensionCount, initArgs.validationLayer.pEnabledLayerName);

        VkInstance instance = nullptr;

        VkApplicationInfo appInfo
        {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = initArgs.rootName,
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = initArgs.rootName,
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = VK_API_VERSION_1_4
        };

        VkInstanceCreateInfo instanceInfo
        {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = initArgs.enabledExtensionCount,
            .ppEnabledExtensionNames = initArgs.ppEnabledExtensionNames
        };

        VkDebugUtilsMessengerCreateInfoEXT validationLayerExt = CreateValidationLayerInfoEXT(initArgs.validationLayer.pUserData);

        if (k_EnableValidationLayers &&
            initArgs.validationLayer.pEnabledLayerName)
        {
            instanceInfo.pNext = &validationLayerExt;
            instanceInfo.enabledLayerCount = 1;
            instanceInfo.ppEnabledLayerNames = &initArgs.validationLayer.pEnabledLayerName;
        }

        //instanceInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR

        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));
        
        if (k_EnableValidationLayers &&
            initArgs.validationLayer.pEnabledLayerName)
        {
            VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &validationLayerExt, nullptr, &debugMessenger));
        }

        return instance;
    }

    static void DestroyInstance(
        const VkInstance& instance,
        const VkDebugUtilsMessengerEXT& debugMessenger) 
    {
        if (k_EnableValidationLayers) 
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger,  nullptr);
        }
        vkDestroyInstance(instance, nullptr);
    }

    static void CheckDevice(const VkPhysicalDevice& physical,
        const char* const* extensions, uint32_t count) 
    {
        const auto deviceEXTs = Enumerator::Extensions::get_device(physical);

        for (uint32_t i = 0; i < count; ++i)
        {
            const char* extension = extensions[i];
            if (std::find_if(deviceEXTs.cbegin(), deviceEXTs.cend(),
                [extension](const  std::string& extStr)
                {
                    return extStr == extension;
                }) == deviceEXTs.cend())
            {
                std::ostringstream ss;
                ss << "[CheckDevice] Device extension: " << std::quoted(extension)
                    << " not found";

                throw std::runtime_error(ss.str());
            }
        }
    }

    GraphicsContext::GraphicsContext(const WindowBase& window) : m_impl(new Impl)
    {
        // ===================================================================
        // Create Vulkan instance (core API entry point)
        InstanceCreationArgs initArgs
        {
            .rootName = "TUNGOL",
            
            .validationLayer = ValidationLayerInfo
            {
                .pUserData = this,
                .pEnabledLayerName = k_ValidationLayerName
            },

            .enabledExtensionCount = k_InstanceExtensionCount,
            .ppEnabledExtensionNames = k_InstanceExtensions
        };
        m_impl->instance = CreateInstance(initArgs, m_impl->debugMessenger);
        // ===================================================================


        // ===================================================================
        // Create the surface
        VK_CHECK(SurfaceProvider::createSurface(m_impl->instance, window, &m_impl->surface));
        // ===================================================================


         // ===================================================================
        // Load all physical devices
        Enumerator::PhysicalDevices::init(m_impl->instance);
        {
            PhysicalDeviceDesc deviceDesc;

            try 
            {
                // Select the preferred GPU (discrete) VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                deviceDesc = Enumerator::PhysicalDevices::get(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

                fprintf(stderr, "[GraphicsContext::GraphicsContext] Selected discrete GPU: %s\n", deviceDesc.name.c_str());
            }
            catch (std::exception& e) 
            {
                // Select the fallback GPU (integrated) VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                deviceDesc = Enumerator::PhysicalDevices::get(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
                
                fprintf(stderr, "[GraphicsContext::GraphicsContext] %s\n", e.what());
                fprintf(stderr, "[GraphicsContext::GraphicsContext] Selected integrated GPU: %s\n", deviceDesc.name.c_str());
            }

           /* if (deviceDesc.properties.apiVersion < )
                throw std::runtime_error("");*/

            fprintf(stderr, "[GraphicsContext::GraphicsContext] apiVersion: %u.%u.%u\n", VK_VERSION_MAJOR(deviceDesc.properties.apiVersion),
                VK_VERSION_MINOR(deviceDesc.properties.apiVersion), VK_VERSION_PATCH(deviceDesc.properties.apiVersion));
            
            // Retrieve queue family details
            const auto queueFamilies = Enumerator::QueueFamilyIndices::list(deviceDesc.physical, m_impl->surface);

            CheckDevice(deviceDesc.physical, k_DeviceExtensions, k_DeviceExtensionCount);

            DeviceDesc desc
            {
                .description = deviceDesc,
                .queueFamilies = queueFamilies,

                .enabledExtensionCount = k_DeviceExtensionCount,
                .ppEnabledExtensionNames = k_DeviceExtensions
            };

            // Create the logical device and queues
            m_impl->device = std::make_unique<Device>(desc);
            
            // Create the swapchain based on surface capabilities
            auto surfaceDetails = Enumerator::Swapchain::get(deviceDesc.physical, m_impl->surface);
            m_impl->swapchain = std::make_unique<Swapchain>(*m_impl->device, surfaceDetails);
            m_impl->physical = deviceDesc.physical;
        }
        // ===================================================================
    }

    GraphicsContext::~GraphicsContext() 
    {
        if (m_impl) 
        {
            m_impl->swapchain.reset();
            m_impl->device.reset();
            SurfaceProvider::destroySurface(m_impl->instance, m_impl->surface);
            DestroyInstance(m_impl->instance, m_impl->debugMessenger);
        }
    }
    Device& GraphicsContext::activeDevice() const
    {
        if (!m_impl)
            throw std::runtime_error("[GraphicsContext::activeDevice] Context is nullptr.");

        if (!m_impl->device)
            throw std::runtime_error("[GraphicsContext::activeDevice] Device was never created.");

        return *m_impl->device;
    }

    Swapchain& GraphicsContext::swapchain() const
    {
        if (!m_impl)
            throw std::runtime_error("[GraphicsContext::swapchain] Context is nullptr.");

        if (!m_impl->swapchain)
            throw std::runtime_error("[GraphicsContext::swapchain] Swapchain was never created.");

        return *m_impl->swapchain;
    }

    void GraphicsContext::rebuildSwapchain()
    {
        if (m_impl) 
        {
            std::unique_ptr<Swapchain> oldSwapchain = std::move(m_impl->swapchain);
            auto surfaceDetails = Enumerator::Swapchain::get(m_impl->physical, m_impl->surface);
            m_impl->swapchain = std::make_unique<Swapchain>(*m_impl->device, surfaceDetails, oldSwapchain.get());
            activeDevice().wait();
            oldSwapchain.reset();
        }
    }

  

}

