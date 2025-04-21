#pragma once

#include "vulkan_engine.h"
#include <chrono>
#include <thread>
#include <iostream> // For error logging

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
    vkShaderHelper_.release();
    vkWindowHelper_.release();
    vkQueueHelper_.release();
    vkDeviceHelper_.release();
    vkInstanceHelper_.release();
}

// Initialize the engine
void VulkanEngine::InitializeSDL()
{
    SVulkanSDLWindowConfig window_config;
    window_config.window_name_ = engine_config_.window_config.title.c_str();
    window_config.width_ = engine_config_.window_config.width;
    window_config.height_ = engine_config_.window_config.height;
    window_config.window_flags_ = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
    window_config.init_flags_ = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    vkWindowHelper_ = std::make_unique<VulkanSDLWindowHelper>(window_config);
}

void VulkanEngine::InitializeVulkan()
{
    if (!CreateInstance()) {
        Logger::LogError("Failed to create Vulkan instance.");
        return;
    }

    if (!CreateSurface()) {
        Logger::LogError("Failed to create Vulkan surface.");
        return;
    }

    if (!CreatePhysicalDevice()) {
        Logger::LogError("Failed to create Vulkan physical device.");
        return;
    }

    if (!CreateLogicalDevice()) {
        Logger::LogError("Failed to create Vulkan logical device.");
        return;
    }

    if (!CreateSwapChain()) {
        Logger::LogError("Failed to create Vulkan swap chain.");
        return;
    }

    if (!CreatePipeline()) {
        Logger::LogError("Failed to create Vulkan pipeline.");
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

// ------------------------------------
// private function to create the engine
// ------------------------------------

bool VulkanEngine::CreateInstance()
{
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
    instance_config.validation_layers = { "VK_LAYER_KHRONOS_validation" }; // TODO: Make configurable
    instance_config.extensions = extensions;
    vkInstanceHelper_ = std::make_unique<VulkanInstanceHelper>(instance_config);

    return vkInstanceHelper_->CreateVulkanInstance();
}

bool VulkanEngine::CreateSurface()
{
    return vkWindowHelper_->CreateSurface(&vkInstanceHelper_->GetVulkanInstance());
}

bool VulkanEngine::CreatePhysicalDevice()
{
    SVulkanPhysicalDeviceConfig physical_device_config;
    physical_device_config.physical_device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; // TODO: Make configurable
    physical_device_config.physical_device_api_version[0] = 0;
    physical_device_config.physical_device_api_version[1] = 1;
    physical_device_config.physical_device_api_version[2] = 3;
    physical_device_config.physical_device_api_version[3] = 0;
    physical_device_config.queue_flags = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT }; // TODO: Make configurable
    physical_device_config.physical_device_features = { GeometryShader }; // TODO: Make configurable

    vkDeviceHelper_ = std::make_unique<VulkanDeviceHelper>();
    return vkDeviceHelper_->CreatePhysicalDevice(physical_device_config, vkInstanceHelper_->GetVulkanInstance());
}

bool VulkanEngine::CreateLogicalDevice()
{
    // get swapchain required extensions
    vkSwapChainHelper_ = std::make_unique<VulkanSwapChainHelper>();
    std::vector<const char*> swapchain_required_extensions;
    int swapchain_required_extension_count = vkSwapChainHelper_->GetSwapChainExtensions(vkDeviceHelper_->GetPhysicalDevice(), swapchain_required_extensions);

    // create queue
    SVulkanQueueConfig queue_config;
    queue_config.queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT; // TODO: Make configurable
    vkQueueHelper_ = std::make_unique<VulkanQueueHelper>(queue_config);
    vkQueueHelper_->PickQueueFamily(vkDeviceHelper_->GetPhysicalDevice(), vkWindowHelper_->GetSurface());
    VkDeviceQueueCreateInfo queue_create_info = {};
    vkQueueHelper_->GenerateQueueCreateInfo(queue_create_info);

    // create logical device
    SVulkanDeviceConfig device_config;
    device_config.queue_create_infos.push_back(queue_create_info);
    device_config.device_extensions = swapchain_required_extensions;
    device_config.device_extension_count = static_cast<int>(swapchain_required_extension_count);
    return vkDeviceHelper_->CreateLogicalDevice(device_config);
}

bool VulkanEngine::CreateSwapChain()
{
    SVulkanSwapChainConfig swap_chain_config;
    swap_chain_config.target_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM; // TODO: Make configurable or query best
    swap_chain_config.target_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // TODO: Make configurable or query best
    swap_chain_config.target_present_mode_ = VK_PRESENT_MODE_FIFO_KHR; // TODO: Make configurable (Mailbox is often preferred)
    swap_chain_config.target_swap_extent_.width = engine_config_.window_config.width; // TODO: Handle resize
    swap_chain_config.target_swap_extent_.height = engine_config_.window_config.height; // TODO: Handle resize
    swap_chain_config.target_image_count_ = engine_config_.frame_count;

    vkSwapChainHelper_->Setup(swap_chain_config,
        vkDeviceHelper_->GetLogicalDevice(),
        vkDeviceHelper_->GetPhysicalDevice(),
        vkWindowHelper_->GetSurface(),
        vkWindowHelper_->GetWindow());

    return vkSwapChainHelper_->CreateSwapChain();
}

bool VulkanEngine::CreatePipeline()
{
    // create shader
    vkShaderHelper_ = std::make_unique<VulkanShaderHelper>(vkDeviceHelper_->GetLogicalDevice());

    std::vector<SVulkanShaderConfig> configs;
    std::string shader_path = engine_config_.general_config.working_directory + "\\src\\shader\\";
    std::string vertex_shader_path = shader_path + "triangle.vert.spv";
    std::string fragment_shader_path = shader_path + "triangle.frag.spv";
    configs.push_back({ EShaderType::VERTEX_SHADER, vertex_shader_path.c_str() });
    configs.push_back({ EShaderType::FRAGMENT_SHADER, fragment_shader_path.c_str() });

    for (const auto& config : configs)
    {
        std::vector<uint32_t> shader_code;
        if (!vkShaderHelper_->ReadShaderCode(config.shader_path, shader_code))
        {
            Logger::LogError("Failed to read shader code from " + std::string(config.shader_path));
            return false;
        }

        if (!vkShaderHelper_->CreateShaderModule(vkDeviceHelper_->GetLogicalDevice(), shader_code, config.shader_type))
        {
            Logger::LogError("Failed to create shader module for " + std::string(config.shader_path));
            return false;
        }
    }

    // create renderpass
    SVulkanRenderpassConfig renderpass_config;
    auto swapchain_config = vkSwapChainHelper_->GetSwapChainConfig();
    renderpass_config.color_format = swapchain_config->target_surface_format_.format;
    renderpass_config.depth_format = VK_FORMAT_D32_SFLOAT; // TODO: Make configurable
    renderpass_config.sample_count = VK_SAMPLE_COUNT_1_BIT; // TODO: Make configurable
    vkRenderpassHelper_ = std::make_unique<VulkanRenderpassHelper>(renderpass_config);
    if (!vkRenderpassHelper_->CreateRenderpass(vkDeviceHelper_->GetLogicalDevice()))
    {
        Logger::LogError("Failed to create renderpass");
        return false;
    }

    // create pipeline
    SVulkanPipelineConfig pipeline_config;
    pipeline_config.swap_chain_config = swapchain_config;
    pipeline_config.shader_module_map = {
        { EShaderType::VERTEX_SHADER, vkShaderHelper_->GetShaderModule(EShaderType::VERTEX_SHADER) },
        { EShaderType::FRAGMENT_SHADER, vkShaderHelper_->GetShaderModule(EShaderType::FRAGMENT_SHADER) }
    };
    pipeline_config.renderpass = vkRenderpassHelper_->GetRenderpass();
    VulkanPipelineHelper pipeline_helper(pipeline_config);
    if (!pipeline_helper.CreatePipeline(vkDeviceHelper_->GetLogicalDevice()))
    {
        Logger::LogError("Failed to create pipeline");
        return false;
    }
}
