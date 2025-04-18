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
    vkSwapChainHelper_.release();
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

    auto window_required_extensions = vkWindowHelper_->GetWindowExtensions();
    int window_required_extension_count = vkWindowHelper_->GetWindowExtensionCount();

    auto extensions = std::vector<const char*>(window_required_extensions, window_required_extensions + window_required_extension_count);

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
    instance_config.extensions = extensions;
    vkInstanceHelper_ = std::make_unique<VulkanInstanceHelper>(instance_config);
    
    if (!vkInstanceHelper_->CreateVulkanInstance()) {
        return;
    }

    // create surface

    vkWindowHelper_->CreateSurface(&vkInstanceHelper_->GetVulkanInstance());

    // create physical device

    SVulkanPhysicalDeviceConfig physical_device_config;
    physical_device_config.physical_device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    physical_device_config.physical_device_api_version[0] = 0;
    physical_device_config.physical_device_api_version[1] = 1;
    physical_device_config.physical_device_api_version[2] = 3;
    physical_device_config.physical_device_api_version[3] = 0;
    physical_device_config.queue_flags = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT };
    physical_device_config.physical_device_features = { GeometryShader };

    vkDeviceHelper_ = std::make_unique<VulkanDeviceHelper>();
    if (!vkDeviceHelper_->CreatePhysicalDevice(physical_device_config, vkInstanceHelper_->GetVulkanInstance())) {
         return;
    }


    // create default swap chain helper to get swap chain extensions

    vkSwapChainHelper_ = std::make_unique<VulkanSwapChainHelper>();
    std::vector<const char*> swapchain_required_extensions;
    int swapchain_required_extension_count = vkSwapChainHelper_->GetSwapChainExtensions(vkDeviceHelper_->GetPhysicalDevice(), swapchain_required_extensions);

    // create queue

    SVulkanQueueConfig queue_config;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_config.queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    vkQueueHelper_ = std::make_unique<VulkanQueueHelper>(queue_config);
    vkQueueHelper_->PickQueueFamily(vkDeviceHelper_->GetPhysicalDevice(), vkWindowHelper_->GetSurface());
    vkQueueHelper_->GenerateQueueCreateInfo(queue_create_info);

    // create logical device
    SVulkanDeviceConfig device_config;
    device_config.queue_create_infos.push_back(queue_create_info);
    device_config.device_extensions = swapchain_required_extensions;
    device_config.device_extension_count = static_cast<int>(swapchain_required_extension_count);
    if (!vkDeviceHelper_->CreateLogicalDevice(device_config)) {
        return;
    }

    // create swap chain

    SVulkanSwapChainConfig swap_chain_config;
    swap_chain_config.target_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM;
    swap_chain_config.target_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swap_chain_config.target_present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
    swap_chain_config.target_swap_extent_.width = engine_config_.window.width;
    swap_chain_config.target_swap_extent_.height = engine_config_.window.height;
    swap_chain_config.target_image_count_ = engine_config_.frame_count;
    vkSwapChainHelper_->Setup(swap_chain_config, 
                              vkDeviceHelper_->GetLogicalDevice(), 
                              vkDeviceHelper_->GetPhysicalDevice(), 
                              vkWindowHelper_->GetSurface(), 
                              vkWindowHelper_->GetWindow());
    if (!vkSwapChainHelper_->CreateSwapChain()) {
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
