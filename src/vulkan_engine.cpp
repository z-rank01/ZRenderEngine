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
    ShutdownSDL();
}

// Initialize the engine
void VulkanEngine::InitializeSDL()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    // auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);    // failed when there is no vulkan driver
    auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE);
    window_ = SDL_CreateWindow("Vulkan Engine", engine_config_.window.width, engine_config_.window.height, window_flags);
    renderer_ = SDL_CreateRenderer(window_, "Vulkan Renderer");

    // show window (has implicitly created after SDL_CreateWindow)
    SDL_ShowWindow(window_);
}

void VulkanEngine::InitializeVulkan()
{
    // TODO: Use builder pattern to create VulkanInstanceHelper
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
    instance_config.extensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    vkInstanceHelper_ = std::make_unique<VulkanInstanceHelper>(instance_config);
    vkInstanceHelper_->CreateVulkanInstance();

    SVulkanDeviceConfig device_config;
    device_config.physical_device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    device_config.physical_device_api_version[0] = 0;
    device_config.physical_device_api_version[1] = 1;
    device_config.physical_device_api_version[2] = 3;
    device_config.physical_device_api_version[3] = 0;
    device_config.queue_flags = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT };
    device_config.physical_device_features = { };

    vkDeviceHelper_ = std::make_unique<VulkanDeviceHelper>(device_config);
    vkDeviceHelper_->CreatePhysicalDevice(vkInstanceHelper_->GetVulkanInstance());
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
    SDL_RenderClear(renderer_);
    // TODO: Add your rendering code here
    SDL_RenderTexture(renderer_, nullptr, 0, 0);
    SDL_RenderPresent(renderer_);  // 显示渲染的内容
}

// Shutdown the engine
void VulkanEngine::ShutdownSDL()
{
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();

    instance = nullptr;
}
