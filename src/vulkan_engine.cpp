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
    vkShaderHelper_.release();
    vkWindowHelper_.release();
    vkRenderpassHelper_.release();
    vkPipelineHelper_.release();
    vkFrameBufferHelper_.release();
    vkCommandBufferHelper_.release();
    vkSynchronizationHelper_.release();

    vkb::destroy_swapchain(vkb_swapchain_);
    vkb::destroy_device(vkb_device_);
    vkb::destroy_instance(vkb_instance_);
}

// Initialize the engine
void VulkanEngine::InitializeSDL()
{
    vkWindowHelper_ = std::make_unique<VulkanSDLWindowHelper>();
    if (!vkWindowHelper_->GetWindowBuilder()
        .SetWindowName(engine_config_.window_config.title.c_str())
        .SetWindowSize(engine_config_.window_config.width, engine_config_.window_config.height)
        .SetWindowFlags(SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN)
        .SetInitFlags(SDL_INIT_VIDEO | SDL_INIT_EVENTS)
        .Build())
    {
        throw std::runtime_error("Failed to create SDL window.");
    }
}

void VulkanEngine::InitializeVulkan()
{
    GenerateFrameStructs();

    if (!CreateInstance()) {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }

    if (!CreateSurface()) {
        throw std::runtime_error("Failed to create Vulkan surface.");
    }

    if (!CreatePhysicalDevice()) {
        throw std::runtime_error("Failed to create Vulkan physical device.");
    }

    if (!CreateLogicalDevice()) {
        throw std::runtime_error("Failed to create Vulkan logical device.");
    }

    if (!CreateSwapChain()) {
        throw std::runtime_error("Failed to create Vulkan swap chain.");
    }

    if (!CreatePipeline()) {
        throw std::runtime_error("Failed to create Vulkan pipeline.");
    }

    if (!CreateFrameBuffer()) {
        throw std::runtime_error("Failed to create Vulkan frame buffer.");
    }

    if (!CreateCommandPool()) {
        throw std::runtime_error("Failed to create Vulkan command pool.");
    }

    if (!AllocateCommandBuffer()) {
        throw std::runtime_error("Failed to allocate Vulkan command buffer.");
    }

    if (!CreateSynchronizationObjects()) {
        throw std::runtime_error("Failed to create Vulkan synchronization objects.");
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

        if (resize_request_)
        {
            ResizeSwapChain();
        }

        Draw();
    }
}

// Main render loop
void VulkanEngine::Draw()
{
    DrawFrame();
}

// ------------------------------------
// private function to create the engine
// ------------------------------------

void VulkanEngine::GenerateFrameStructs()
{
    output_frames_.resize(engine_config_.frame_count);
    for (int i = 0; i < engine_config_.frame_count; ++i) {
        output_frames_[i].image_index = i;
        output_frames_[i].queue_id = "graphic_queue";
        output_frames_[i].command_buffer_id = "graphic_command_buffer_" + std::to_string(i);
        output_frames_[i].image_available_sempaphore_id = "image_available_semaphore_" + std::to_string(i);
        output_frames_[i].render_finished_sempaphore_id = "render_finished_semaphore_" + std::to_string(i);
        output_frames_[i].fence_id = "in_flight_fence_" + std::to_string(i);
    }
}

bool VulkanEngine::CreateInstance()
{
    vkb::InstanceBuilder builder;
    
    auto inst_ret = builder
        .set_app_name(engine_config_.window_config.title.c_str())
        .set_engine_name("Vulkan Engine")
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
        .enable_validation_layers(engine_config_.use_validation_layers)
        .build();

    if (!inst_ret) {
        std::cout << "Failed to create Vulkan instance. Error: " << inst_ret.error().message() << std::endl;
        return false;
    }

    vkb_instance_ = inst_ret.value();
    return true;
}

bool VulkanEngine::CreateSurface()
{
    return vkWindowHelper_->CreateSurface(vkb_instance_);
}

bool VulkanEngine::CreatePhysicalDevice()
{
    //vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features_13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features_13.synchronization2 = true;

    vkb::PhysicalDeviceSelector selector{vkb_instance_};
    auto phys_ret = selector
        .set_surface(vkWindowHelper_->GetSurface())
        .set_minimum_version(1, 3)
        .set_required_features_13(features_13)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .require_present()
        .select();

    if (!phys_ret) {
        std::cout << "Failed to select Vulkan Physical Device. Error: " << phys_ret.error().message() << std::endl;
        return false;
    }

    vkb_physical_device_ = phys_ret.value();
    return true;
}

bool VulkanEngine::CreateLogicalDevice()
{
    vkb::DeviceBuilder device_builder{vkb_physical_device_};
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        std::cout << "Failed to create Vulkan device. Error: " << dev_ret.error().message() << std::endl;
        return false;
    }

    vkb_device_ = dev_ret.value();
    return true;
}

bool VulkanEngine::CreateSwapChain()
{
    vkb::SwapchainBuilder swapchain_builder{vkb_device_};
    auto swap_ret = swapchain_builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(engine_config_.window_config.width, engine_config_.window_config.height)
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();

    if (!swap_ret) {
        std::cout << "Failed to create Vulkan swapchain. Error: " << swap_ret.error().message() << std::endl;
        return false;
    }

    vkb_swapchain_ = swap_ret.value();

    // fill in swapchain config
    swapchain_config_.target_surface_format_.format = vkb_swapchain_.image_format;
    swapchain_config_.target_surface_format_.colorSpace = vkb_swapchain_.color_space;
    swapchain_config_.target_present_mode_ = vkb_swapchain_.present_mode;
    swapchain_config_.target_swap_extent_ = vkb_swapchain_.extent;
    swapchain_config_.target_image_count_ = vkb_swapchain_.image_count;
    swapchain_config_.device_extensions_.clear();
    swapchain_config_.device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    return true;
}

bool VulkanEngine::CreatePipeline()
{
    // create shader
    vkShaderHelper_ = std::make_unique<VulkanShaderHelper>(vkb_device_);

    std::vector<SVulkanShaderConfig> configs;
    std::string shader_path = engine_config_.general_config.working_directory + "src\\shader\\";
    std::string vertex_shader_path = shader_path + "triangle.vert.spv";
    std::string fragment_shader_path = shader_path + "triangle.frag.spv";
    configs.push_back({ EShaderType::kVertexShader, vertex_shader_path.c_str() });
    configs.push_back({ EShaderType::kFragmentShader, fragment_shader_path.c_str() });

    for (const auto& config : configs)
    {
        std::vector<uint32_t> shader_code;
        if (!vkShaderHelper_->ReadShaderCode(config.shader_path, shader_code))
        {
            Logger::LogError("Failed to read shader code from " + std::string(config.shader_path));
            return false;
        }

        if (!vkShaderHelper_->CreateShaderModule(vkb_device_, shader_code, config.shader_type))
        {
            Logger::LogError("Failed to create shader module for " + std::string(config.shader_path));
            return false;
        }
    }

    // create renderpass
    SVulkanRenderpassConfig renderpass_config;
    renderpass_config.color_format = vkb_swapchain_.image_format;
    renderpass_config.depth_format = VK_FORMAT_D32_SFLOAT; // TODO: Make configurable
    renderpass_config.sample_count = VK_SAMPLE_COUNT_1_BIT; // TODO: Make configurable
    vkRenderpassHelper_ = std::make_unique<VulkanRenderpassHelper>(renderpass_config);
    if (!vkRenderpassHelper_->CreateRenderpass(vkb_device_))
    {
        return false;
    }

    // create pipeline
    SVulkanPipelineConfig pipeline_config;
    pipeline_config.swap_chain_config = &swapchain_config_;
    pipeline_config.shader_module_map = {
        { EShaderType::kVertexShader, vkShaderHelper_->GetShaderModule(EShaderType::kVertexShader) },
        { EShaderType::kFragmentShader, vkShaderHelper_->GetShaderModule(EShaderType::kFragmentShader) }
    };
    pipeline_config.renderpass = vkRenderpassHelper_->GetRenderpass();
    vkPipelineHelper_ = std::make_unique<VulkanPipelineHelper>(pipeline_config);
    return vkPipelineHelper_->CreatePipeline(vkb_device_);
}

bool VulkanEngine::CreateFrameBuffer()
{
    // create frame buffer
    auto swapchain_config = &swapchain_config_;
    auto swapchain_image_views = vkb_swapchain_.get_image_views().value();

    SVulkanFrameBufferConfig framebuffer_config;
    framebuffer_config.extent = swapchain_config->target_swap_extent_;
    framebuffer_config.image_views = &swapchain_image_views;
    vkFrameBufferHelper_ = std::make_unique<VulkanFrameBufferHelper>(framebuffer_config);

    return vkFrameBufferHelper_->CreateFrameBuffer(vkb_device_, vkRenderpassHelper_->GetRenderpass());
}

bool VulkanEngine::CreateCommandPool()
{
    vkCommandBufferHelper_ = std::make_unique<VulkanCommandBufferHelper>();
    return vkCommandBufferHelper_->CreateCommandPool(vkb_device_, vkb_device_.get_queue_index(vkb::QueueType::graphics).value());
}

bool VulkanEngine::AllocateCommandBuffer()
{
    for(int i = 0; i < engine_config_.frame_count; ++i)
    {
        if (!vkCommandBufferHelper_->AllocateCommandBuffer({ VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 }, output_frames_[i].command_buffer_id))
        {
            Logger::LogError("Failed to allocate command buffer for frame " + std::to_string(i));
            return false;
        }
    }
    return true;
}

bool VulkanEngine::CreateSynchronizationObjects()
{
    vkSynchronizationHelper_ = std::make_unique<VulkanSynchronizationHelper>(vkb_device_);
    // create synchronization objects
    for(int i = 0; i < engine_config_.frame_count; ++i)
    {
        if (!vkSynchronizationHelper_->CreateSemaphore(output_frames_[i].image_available_sempaphore_id)) return false;
        if (!vkSynchronizationHelper_->CreateSemaphore(output_frames_[i].render_finished_sempaphore_id)) return false;
        if (!vkSynchronizationHelper_->CreateFence(output_frames_[i].fence_id)) return false;
    }
    return true;
}


// ----------------------------------
// private function to draw the frame
// ----------------------------------

void VulkanEngine::DrawFrame()
{
    // get current resource
    auto current_fence_id = output_frames_[frame_index_].fence_id;
    auto current_image_available_semaphore_id = output_frames_[frame_index_].image_available_sempaphore_id;
    auto current_render_finished_semaphore_id = output_frames_[frame_index_].render_finished_sempaphore_id;
    auto current_command_buffer_id = output_frames_[frame_index_].command_buffer_id;
    auto current_queue_id = output_frames_[frame_index_].queue_id;
    
    // wait for last frame to finish
    if (!vkSynchronizationHelper_->WaitForFence(current_fence_id)) return;

    // get semaphores
    auto image_available_semaphore = vkSynchronizationHelper_->GetSemaphore(current_image_available_semaphore_id);
    auto render_finished_semaphore = vkSynchronizationHelper_->GetSemaphore(current_render_finished_semaphore_id);
    auto in_flight_fence = vkSynchronizationHelper_->GetFence(current_fence_id);

    // acquire next image
    uint32_t image_index;
    if (!Logger::LogWithVkResult(vkAcquireNextImageKHR(vkb_device_.device, vkb_swapchain_.swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index), 
        "Failed to acquire next image", 
        "Succeeded in acquiring next image"))
    {
        return;
    }

    // reset fence before submitting
    if (!vkSynchronizationHelper_->ResetFence(current_fence_id)) return;

    // record command buffer
    if (!vkCommandBufferHelper_->ResetCommandBuffer(current_command_buffer_id)) return;
    if (!RecordCommand(image_index, current_command_buffer_id)) return;

    // submit command buffer
    VkCommandBufferSubmitInfo command_buffer_submit_info{};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = vkCommandBufferHelper_->GetCommandBuffer(current_command_buffer_id);

    VkSemaphoreSubmitInfo wait_semaphore_info{};
    wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_semaphore_info.semaphore = image_available_semaphore;
    wait_semaphore_info.value = 1;

    VkSemaphoreSubmitInfo signal_semaphore_info{};
    signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_semaphore_info.semaphore = render_finished_semaphore;
    signal_semaphore_info.value = 1;

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.waitSemaphoreInfoCount = 1;
    submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
    if (!Logger::LogWithVkResult(vkQueueSubmit2(vkb_device_.get_queue(vkb::QueueType::graphics).value(), 1, &submit_info, in_flight_fence),
        "Failed to submit command buffer",
        "Succeeded in submitting command buffer"))
    {
        return;
    }

    // present the image
    SVulkanQueuePresentConfig present_config;
    present_config.queue_id = current_queue_id;
    present_config.swapchains.push_back(vkb_swapchain_.swapchain);
    present_config.image_indices.push_back(image_index);
    present_config.wait_semaphores.push_back(render_finished_semaphore);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vkb_swapchain_.swapchain;
    present_info.pImageIndices = &image_index;
    if (!Logger::LogWithVkResult(vkQueuePresentKHR(vkb_device_.get_queue(vkb::QueueType::graphics).value(), &present_info),
        "Failed to present image",
        "Succeeded in presenting image"))
    {
        return;
    }

    // update frame index
    frame_index_ = (frame_index_ + 1) % engine_config_.frame_count;
}

void VulkanEngine::ResizeSwapChain()
{
    // wait for the device to be idle
    vkDeviceWaitIdle(vkb_device_.device);

    // destroy old swapchain
    vkDestroySwapchainKHR(vkb_device_.device, vkb_swapchain_.swapchain, nullptr);

    // reset window size
    auto current_extent = vkWindowHelper_->GetCurrentWindowExtent();
    engine_config_.window_config.width = current_extent.width;
    engine_config_.window_config.height = current_extent.height;

    // create new swapchain
    if (!CreateSwapChain()) {
        throw std::runtime_error("Failed to create Vulkan swap chain.");
    }

    // recreate framebuffers
    if (!CreateFrameBuffer()) {
        throw std::runtime_error("Failed to create Vulkan frame buffer.");
    }

    // recreate command buffers
    if (!AllocateCommandBuffer()) {
        throw std::runtime_error("Failed to allocate Vulkan command buffer.");
    }

    resize_request_ = false;
}

bool VulkanEngine::RecordCommand(uint32_t image_index, std::string command_buffer_id)
{
    // begin command recording
    if (!vkCommandBufferHelper_->BeginCommandBufferRecording(command_buffer_id, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        return false;

    // collect needed objects
    auto commandBuffer = vkCommandBufferHelper_->GetCommandBuffer(command_buffer_id);
    auto swapchain_config = &swapchain_config_;

    // begin renderpass
    VkClearValue clear_color = {};
    clear_color.color.float32[0] = 0.1f;
    clear_color.color.float32[1] = 0.1f;
    clear_color.color.float32[2] = 0.1f;
    clear_color.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = vkRenderpassHelper_->GetRenderpass();
    renderpass_info.framebuffer = (*vkFrameBufferHelper_->GetFramebuffers())[image_index];
    renderpass_info.renderArea.offset = { 0, 0 };
    renderpass_info.renderArea.extent = swapchain_config->target_swap_extent_;
    renderpass_info.clearValueCount = 1;
    renderpass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(commandBuffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

    // bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineHelper_->GetPipeline());

    // dynamic state update
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_config->target_swap_extent_.width);
    viewport.height = static_cast<float>(swapchain_config->target_swap_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_config->target_swap_extent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // draw
    vkCmdDraw(commandBuffer, 3, 1, 0, 0); // TODO: Handle vertex buffer and index buffer

    // end renderpass
    vkCmdEndRenderPass(commandBuffer);

    // end command recording
    if (!vkCommandBufferHelper_->EndCommandBufferRecording(command_buffer_id))
        return false;

    return true;
}
