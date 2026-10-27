#pragma once
#include <event/EventBus.hpp>
#include <event/GraphicsEventArgs.hpp>

#include <stdexcept>


#define CATCH_GRAPHICS_ERROR \
    catch (const std::exception& e) { \
        std::string msg = std::string(e.what()) \
            + " [" __FILE__ ":" + std::to_string(__LINE__) + "]"; \
        tungol::event::GraphicsEventArgs g( \
            tungol::event::GraphicsAPI::Vulkan, \
            tungol::event::MessageType::Error, \
            msg); \
        tungol::event::EventBus::instance().fire( \
            tungol::event::EventType::Graphics, g); throw; \
    }