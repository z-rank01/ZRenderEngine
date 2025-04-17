#pragma once

#include "vulkan_engine.h"
#include <chrono>
#include <thread>

VulkanEngine* instance = nullptr;

VulkanEngine& VulkanEngine::GetInstance()
{
    return *instance;
}

VulkanEngine::VulkanEngine(const SEngineConfig& config) : engine_config_(config)
{
    // only one engine initialization is allowed with the application.
    assert(instance == nullptr);
    instance = this;
    
    // Initialize the states
    engine_state_ = EWindowState::Initialized;
    render_state_ = ERenderState::True;
    
    InitializeSDL();
    InitializeVulkan();
}

VulkanEngine::~VulkanEngine()
{
    vkWindowHelper_.release();
    vkQueueHelper_.release();
    vkDeviceHelper_.release();
    vkInstanceHelper_.release();
}

// Initialize the engine
void VulkanEngine::InitializeSDL()
{
    SVulkanSDLWindowConfig window_config;
    window_config.window_name_ = engine_config_.window.title.c_str();
    window_config.width_ = engine_config_.window.width;
    window_config.height_ = engine_config_.window.height;
    window_config.window_flags_ = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
    window_config.init_flags_ = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    vkWindowHelper_ = std::make_unique<VulkanSDLWindowHelper>(window_config);
}

void VulkanEngine::InitializeVulkan()
{
    // TODO: Use builder pattern to create VulkanInstanceHelper
    
    // create instance
    auto extensions = vkWindowHelper_->GetWindowExtensions();
    int extension_count = vkWindowHelper_->GetWindowExtensionCount();
    SVulkanInstanceConfig instance_config;
    instance_config.application_name = "Vulkan Engine";
    instance_config.application_version[0] = 1;
    instance_config.application_version[1] = 0;
    instance_config.application_version[2] = 0;
    instance_config.engine_name = "Vulkan Engine";
    instance_config.engine_version[0] = 1;
    instance_config.engine_version[1] = 0;
    instance_config.engine_version[2] = 0;
    instance_config.api_version[0] = 0;
    instance_config.api_version[1] = 1;
    instance_config.api_version[2] = 3;
    instance_config.api_version[3] = 0;
    instance_config.validation_layers = { "VK_LAYER_KHRONOS_validation" };
    instance_config.extensions = std::vector<const char*>(extensions, extensions + extension_count);
    vkInstanceHelper_ = std::make_unique<VulkanInstanceHelper>(instance_config);
    
    if (!vkInstanceHelper_->CreateVulkanInstance()) {
        Logger::LogError("Failed to create Vulkan instance.");
        // Handle instance creation failure
        return;
    }

    // create surface
    vkWindowHelper_->CreateSurface(&vkInstanceHelper_->GetVulkanInstance());

    // create physical device
    SVulkanDeviceConfig device_config;
    device_config.physical_device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    device_config.physical_device_api_version[0] = 0;
    device_config.physical_device_api_version[1] = 1;
    device_config.physical_device_api_version[2] = 3;
    device_config.physical_device_api_version[3] = 0;
    device_config.queue_flags = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT };
    device_config.physical_device_features = { GeometryShader };
    device_config.device_extensions = std::vector<const char*>(extensions, extensions + extension_count);

    vkDeviceHelper_ = std::make_unique<VulkanDeviceHelper>(device_config);
    if (!vkDeviceHelper_->CreatePhysicalDevice(vkInstanceHelper_->GetVulkanInstance())) {
         return;
    }

    // create queue
    SVulkanQueueConfig queue_config;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_config.queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    vkQueueHelper_ = std::make_unique<VulkanQueueHelper>(queue_config);
    vkQueueHelper_->PickQueueFamily(vkDeviceHelper_->GetPhysicalDevice(), vkWindowHelper_->GetSurface());
    vkQueueHelper_->GenerateQueueCreateInfo(queue_create_info);

    // create logical device
    if (!vkDeviceHelper_->CreateLogicalDevice(queue_create_info)) {
        return;
    }
}

// Main loop
void VulkanEngine::Run()
{
    engine_state_ = EWindowState::Running;

    SDL_Event event;

    // main loop
    while (engine_state_ != EWindowState::Stopped)
    {
        // Handle events on queue
        while (SDL_PollEvent(&event))
        {
            // close the window when user alt-f4s or clicks the X button
            if (event.type == SDL_EVENT_QUIT)
            {
                engine_state_ = EWindowState::Stopped;
            }

            if (event.window.type == SDL_EVENT_WINDOW_SHOWN)
            {
                if (event.window.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    render_state_ = ERenderState::False;
                }
                if (event.window.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    render_state_ = ERenderState::True;
                }
            }
        }

        // do not draw if we are minimized
        if (render_state_ == ERenderState::False)
        {
            // throttle the speed to avoid the endless spinning 
            constexpr auto sleep_duration_ms = 100;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration_ms));
            continue;
        }

        Draw();
    }
}

// Main render loop
void VulkanEngine::Draw()
{
    
}
