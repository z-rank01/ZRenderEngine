#include "vulkan_engine.h"
#include <chrono>
#include <thread>
#include <iostream> // For error logging

VulkanEngine *instance = nullptr;

VulkanEngine &VulkanEngine::GetInstance()
{
    return *instance;
}

VulkanEngine::VulkanEngine(const SEngineConfig &config) : engine_config_(config)
{
    // only one engine initialization is allowed with the application.
    assert(instance == nullptr);
    instance = this;

    InitializeSDL();
    InitializeVulkan();

    // test vra functions
    // TestVraFunctions();
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

    if (!CreateInstance())
    {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }

    if (!CreateSurface())
    {
        throw std::runtime_error("Failed to create Vulkan surface.");
    }

    if (!CreatePhysicalDevice())
    {
        throw std::runtime_error("Failed to create Vulkan physical device.");
    }

    if (!CreateLogicalDevice())
    {
        throw std::runtime_error("Failed to create Vulkan logical device.");
    }

    if (!CreateSwapChain())
    {
        throw std::runtime_error("Failed to create Vulkan swap chain.");
    }

    if (!CreateResourceBuffers())
    {
        throw std::runtime_error("Failed to create Vulkan resource's buffers.");
    }

    if (!CreatePipeline())
    {
        throw std::runtime_error("Failed to create Vulkan pipeline.");
    }

    if (!CreateFrameBuffer())
    {
        throw std::runtime_error("Failed to create Vulkan frame buffer.");
    }

    if (!CreateCommandPool())
    {
        throw std::runtime_error("Failed to create Vulkan command pool.");
    }

    

    if (!AllocatePerFrameCommandBuffer())
    {
        throw std::runtime_error("Failed to allocate Vulkan command buffer.");
    }

    if (!CreateSynchronizationObjects())
    {
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
    for (int i = 0; i < engine_config_.frame_count; ++i)
    {
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

    if (!inst_ret)
    {
        std::cout << "Failed to create Vulkan instance. Error: " << inst_ret.error().message() << std::endl;
        return false;
    }

    vkb_instance_ = inst_ret.value();
    return true;
}

bool VulkanEngine::CreateSurface()
{
    return vkWindowHelper_->CreateSurface(vkb_instance_.instance);
}

bool VulkanEngine::CreatePhysicalDevice()
{
    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features_13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features_13.synchronization2 = true;

    vkb::PhysicalDeviceSelector selector{vkb_instance_};
    auto phys_ret = selector
                        .set_surface(vkWindowHelper_->GetSurface())
                        .set_minimum_version(1, 3)
                        .set_required_features_13(features_13)
                        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                        .require_present()
                        .select();

    if (!phys_ret)
    {
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
    if (!dev_ret)
    {
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

    if (!swap_ret)
    {
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
    vkShaderHelper_ = std::make_unique<VulkanShaderHelper>(vkb_device_.device);

    std::vector<SVulkanShaderConfig> configs;
    std::string shader_path = engine_config_.general_config.working_directory + "src\\shader\\";
    std::string vertex_shader_path = shader_path + "triangle.vert.spv";
    std::string fragment_shader_path = shader_path + "triangle.frag.spv";
    configs.push_back({EShaderType::kVertexShader, vertex_shader_path.c_str()});
    configs.push_back({EShaderType::kFragmentShader, fragment_shader_path.c_str()});

    for (const auto &config : configs)
    {
        std::vector<uint32_t> shader_code;
        if (!vkShaderHelper_->ReadShaderCode(config.shader_path, shader_code))
        {
            Logger::LogError("Failed to read shader code from " + std::string(config.shader_path));
            return false;
        }

        if (!vkShaderHelper_->CreateShaderModule(vkb_device_.device, shader_code, config.shader_type))
        {
            Logger::LogError("Failed to create shader module for " + std::string(config.shader_path));
            return false;
        }
    }

    // create renderpass
    SVulkanRenderpassConfig renderpass_config;
    renderpass_config.color_format = vkb_swapchain_.image_format;
    renderpass_config.depth_format = VK_FORMAT_D32_SFLOAT;  // TODO: Make configurable
    renderpass_config.sample_count = VK_SAMPLE_COUNT_1_BIT; // TODO: Make configurable
    vkRenderpassHelper_ = std::make_unique<VulkanRenderpassHelper>(renderpass_config);
    if (!vkRenderpassHelper_->CreateRenderpass(vkb_device_.device))
    {
        return false;
    }

    // create pipeline
    SVulkanPipelineConfig pipeline_config;
    pipeline_config.swap_chain_config = &swapchain_config_;
    pipeline_config.shader_module_map = {
        {EShaderType::kVertexShader, vkShaderHelper_->GetShaderModule(EShaderType::kVertexShader)},
        {EShaderType::kFragmentShader, vkShaderHelper_->GetShaderModule(EShaderType::kFragmentShader)}};
    pipeline_config.renderpass = vkRenderpassHelper_->GetRenderpass();
    pipeline_config.vertex_input_binding_description = vertex_input_binding_description_;
    pipeline_config.vertex_input_attribute_descriptions = {vertex_input_attribute_position_, vertex_input_attribute_color_};
    vkPipelineHelper_ = std::make_unique<VulkanPipelineHelper>(pipeline_config);
    return vkPipelineHelper_->CreatePipeline(vkb_device_.device);
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

    return vkFrameBufferHelper_->CreateFrameBuffer(vkb_device_.device, vkRenderpassHelper_->GetRenderpass());
}

bool VulkanEngine::CreateCommandPool()
{
    vkCommandBufferHelper_ = std::make_unique<VulkanCommandBufferHelper>();
    return vkCommandBufferHelper_->CreateCommandPool(vkb_device_.device, vkb_device_.get_queue_index(vkb::QueueType::graphics).value());
}

bool VulkanEngine::CreateDescriptorRelatives()
{
    // create descriptor pool

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;

    if (Logger::LogWithVkResult(vkCreateDescriptorPool(vkb_device_.device, &pool_info, nullptr, &descriptor_pool_),
                                "Failed to create descriptor pool",
                                "Succeeded in creating descriptor pool"))
        return false;

    // create descriptor set layout

    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &layout_binding;

    if (Logger::LogWithVkResult(vkCreateDescriptorSetLayout(vkb_device_.device, &layout_info, nullptr, &descriptor_set_layout_),
                                "Failed to create descriptor set layout",
                                "Succeeded in creating descriptor set layout"))
        return false;

    // allocate descriptor set

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    if (Logger::LogWithVkResult(vkAllocateDescriptorSets(vkb_device_.device, &alloc_info, &descriptor_set_),
                                "Failed to allocate descriptor set",
                                "Succeeded in allocating descriptor set"))
        return false;

    return true;
}

bool VulkanEngine::CreateResourceBuffers()
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec3 color;
    };

    // vra and vma members
    vra_data_batcher_ = std::make_unique<vra::VraDataBatcher>(vkb_physical_device_.physical_device);

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = vkb_physical_device_.physical_device;
    allocatorCreateInfo.device = vkb_device_.device;
    allocatorCreateInfo.instance = vkb_instance_.instance;

    vmaCreateAllocator(&allocatorCreateInfo, &vma_allocator_);

    // raw data
    const std::vector<Vertex> vertices =
        {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
    const std::vector<uint16_t> indices =
        {
            0, 1, 2, 2, 3, 0};
    vra::VraRawData vertex_raw_data{
        vertices.data(),                 // pData_
        vertices.size() * sizeof(Vertex) // size_
    };
    vra::VraRawData index_raw_data{
        indices.data(),                   // pData_
        indices.size() * sizeof(uint16_t) // size_
    };

    // TODO: change to dynamic
    vra::ResourceId vertex_data_id = 0;
    vra::ResourceId index_data_id = 1;
    vra::ResourceId staging_vertex_data_id = 2;
    vra::ResourceId staging_index_data_id = 3;

    // vertex buffer create info
    VkBufferCreateInfo vertex_buffer_create_info = {};
    vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc vertex_data_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        vertex_buffer_create_info};

    // index buffer create info
    VkBufferCreateInfo index_buffer_create_info = {};
    index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc index_data_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        index_buffer_create_info};

    // staging buffer create info
    VkBufferCreateInfo staging_buffer_create_info = {};
    staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc staging_data_desc{
        vra::VraDataMemoryPattern::CPU_GPU,
        vra::VraDataUpdateRate::Frequent,
        staging_buffer_create_info};

    // collect and group
    vra_data_batcher_->Collect(vertex_data_desc, vertex_raw_data, vertex_data_id);
    vra_data_batcher_->Collect(index_data_desc, index_raw_data, index_data_id);
    vra_data_batcher_->Collect(staging_data_desc, vertex_raw_data, staging_vertex_data_id);
    vra_data_batcher_->Collect(staging_data_desc, index_raw_data, staging_index_data_id);
    vra_data_batcher_->Group();

    // generate vertex and index buffers and allocate memory
    auto group_data = vra_data_batcher_->GetGroupData("static_local_group");
    if (!group_data || group_data->offsets.size() == 0)
    {
        Logger::LogError("Failed to get group data(From vra)");
        return false;
    }
    const auto &local_buffer_create_info = group_data->data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags("static_local_group");
    if (vmaCreateBuffer(vma_allocator_, &local_buffer_create_info, &alloc_info, &local_buffer_, &local_buffer_allocation_, &local_buffer_allocation_info_) != VK_SUCCESS)
    {
        Logger::LogError("Failed to create buffer(From vma)");
        return false;
    }

    // generate staging buffer and copy data
    auto staging_group_data = vra_data_batcher_->GetGroupData("dynamic_sequential_group");
    if (!staging_group_data || staging_group_data->offsets.size() == 0)
    {
        Logger::LogError("Failed to get staging group data(From vra)");
        return false;
    }
    const auto &host_buffer_create_info = staging_group_data->data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags("dynamic_sequential_group");
    if (vmaCreateBuffer(vma_allocator_, &host_buffer_create_info, &staging_alloc_info, &staging_buffer_, &staging_buffer_allocation_, &staging_buffer_allocation_info_) != VK_SUCCESS)
    {
        Logger::LogError("Failed to create staging buffer(From vma)");
        return false;
    }

    // copy data to staging buffer
    void *mapped_data = nullptr;
    vmaMapMemory(vma_allocator_, staging_buffer_allocation_, &mapped_data);
    memcpy(mapped_data, staging_group_data->consolidated_data.data(), staging_group_data->consolidated_data.size());
    vmaUnmapMemory(vma_allocator_, staging_buffer_allocation_);

    // vertex input binding description
    vertex_input_binding_description_.binding = 0;
    vertex_input_binding_description_.stride = sizeof(Vertex);
    vertex_input_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_input_attribute_position_.location = 0;
    vertex_input_attribute_position_.binding = 0;
    vertex_input_attribute_position_.format = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attribute_position_.offset = 0;

    vertex_input_attribute_color_.location = 1;
    vertex_input_attribute_color_.binding = 0;
    vertex_input_attribute_color_.format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attribute_color_.offset = sizeof(glm::vec2);

    return true;
}

bool VulkanEngine::AllocatePerFrameCommandBuffer()
{
    for (int i = 0; i < engine_config_.frame_count; ++i)
    {
        if (!vkCommandBufferHelper_->AllocateCommandBuffer({VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1}, output_frames_[i].command_buffer_id))
        {
            Logger::LogError("Failed to allocate command buffer for frame " + std::to_string(i));
            return false;
        }
    }
    return true;
}

bool VulkanEngine::CreateSynchronizationObjects()
{
    vkSynchronizationHelper_ = std::make_unique<VulkanSynchronizationHelper>(vkb_device_.device);
    // create synchronization objects
    for (int i = 0; i < engine_config_.frame_count; ++i)
    {
        if (!vkSynchronizationHelper_->CreateSemaphore(output_frames_[i].image_available_sempaphore_id))
            return false;
        if (!vkSynchronizationHelper_->CreateSemaphore(output_frames_[i].render_finished_sempaphore_id))
            return false;
        if (!vkSynchronizationHelper_->CreateFence(output_frames_[i].fence_id))
            return false;
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
    if (!vkSynchronizationHelper_->WaitForFence(current_fence_id))
        return;

    // get semaphores
    auto image_available_semaphore = vkSynchronizationHelper_->GetSemaphore(current_image_available_semaphore_id);
    auto render_finished_semaphore = vkSynchronizationHelper_->GetSemaphore(current_render_finished_semaphore_id);
    auto in_flight_fence = vkSynchronizationHelper_->GetFence(current_fence_id);

    // acquire next image
    uint32_t image_index;
    VkResult acquire_result = vkAcquireNextImageKHR(vkb_device_.device, vkb_swapchain_.swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
    {
        resize_request_ = true;
        return;
    }
    else if (acquire_result != VK_SUCCESS)
    {
        Logger::LogWithVkResult(acquire_result, "Failed to acquire next image", "Succeeded in acquiring next image");
        return;
    }

    // reset fence before submitting
    if (!vkSynchronizationHelper_->ResetFence(current_fence_id))
        return;

    // record command buffer
    if (!vkCommandBufferHelper_->ResetCommandBuffer(current_command_buffer_id))
        return;
    if (!RecordCommand(image_index, current_command_buffer_id))
        return;

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
    VkResult present_result = vkQueuePresentKHR(vkb_device_.get_queue(vkb::QueueType::graphics).value(), &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
    {
        resize_request_ = true;
        return;
    }
    else if (present_result != VK_SUCCESS)
    {
        Logger::LogWithVkResult(present_result, "Failed to present image", "Succeeded in presenting image");
        return;
    }

    // update frame index
    frame_index_ = (frame_index_ + 1) % engine_config_.frame_count;
}

void VulkanEngine::ResizeSwapChain()
{
    // wait for the device to be idle
    vkDeviceWaitIdle(vkb_device_.device);

    // destroy old vulkan objects
    // vkDestroySwapchainKHR(vkb_device_.device, vkb_swapchain_.swapchain, nullptr);
    vkb_swapchain_.destroy_image_views(vkb_swapchain_.get_image_views().value());
    vkb::destroy_swapchain(vkb_swapchain_);

    // reset window size
    auto current_extent = vkWindowHelper_->GetCurrentWindowExtent();
    engine_config_.window_config.width = current_extent.width;
    engine_config_.window_config.height = current_extent.height;

    // create new swapchain
    if (!CreateSwapChain())
    {
        throw std::runtime_error("Failed to create Vulkan swap chain.");
    }

    // recreate framebuffers
    if (!CreateFrameBuffer())
    {
        throw std::runtime_error("Failed to create Vulkan frame buffer.");
    }

    // // recreate command buffers
    // if (!AllocateCommandBuffer()) {
    //     throw std::runtime_error("Failed to allocate Vulkan command buffer.");
    // }

    resize_request_ = false;
}

bool VulkanEngine::RecordCommand(uint32_t image_index, std::string command_buffer_id)
{
    // begin command recording
    if (!vkCommandBufferHelper_->BeginCommandBufferRecording(command_buffer_id, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        return false;

    // collect needed objects
    auto command_buffer = vkCommandBufferHelper_->GetCommandBuffer(command_buffer_id);
    auto swapchain_config = &swapchain_config_;

    // copy buffer from staging to local
    VkBufferCopy buffer_copy_info{};
    buffer_copy_info.srcOffset = 0;
    buffer_copy_info.dstOffset = 0;
    buffer_copy_info.size = staging_buffer_allocation_info_.size;
    vkCmdCopyBuffer(command_buffer, staging_buffer_, local_buffer_, 1, &buffer_copy_info);

    VkBufferMemoryBarrier2 buffer_memory_barrier{};
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    buffer_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    buffer_memory_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    buffer_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    buffer_memory_barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    buffer_memory_barrier.buffer = local_buffer_;
    buffer_memory_barrier.offset = 0;
    buffer_memory_barrier.size = VK_WHOLE_SIZE;
    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &buffer_memory_barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency_info);

    // bind vertex and index buffers
    auto vertex_offset = vra_data_batcher_->GetResourceOffset("static_local_group", 0);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &local_buffer_, &vertex_offset);
    auto index_offset = vra_data_batcher_->GetResourceOffset("static_local_group", 1);
    vkCmdBindIndexBuffer(command_buffer, local_buffer_, index_offset, VK_INDEX_TYPE_UINT16);

    

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
    renderpass_info.renderArea.offset = {0, 0};
    renderpass_info.renderArea.extent = swapchain_config->target_swap_extent_;
    renderpass_info.clearValueCount = 1;
    renderpass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

    // bind pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineHelper_->GetPipeline());

    // dynamic state update
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_config->target_swap_extent_.width);
    viewport.height = static_cast<float>(swapchain_config->target_swap_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_config->target_swap_extent_;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // draw
    // vkCmdDraw(command_buffer, 3, 1, 0, 0); // TODO: Handle vertex buffer and index buffer
    vkCmdDrawIndexed(command_buffer, 3, 1, 0, 0, 0);

    // end renderpass
    vkCmdEndRenderPass(command_buffer);

    // end command recording
    if (!vkCommandBufferHelper_->EndCommandBufferRecording(command_buffer_id))
        return false;

    return true;
}

/// --------------------------------
/// test vra functions
/// --------------------------------

void VulkanEngine::TestVraFunctions()
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec3 color;
    };

    // raw data
    const std::vector<Vertex> vertices =
        {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
    const std::vector<uint16_t> indices =
        {
            0, 1, 2, 2, 3, 0};
    vra::VraRawData vertex_raw_data{
        vertices.data(),                 // pData_
        vertices.size() * sizeof(Vertex) // size_
    };
    vra::VraRawData index_raw_data{
        indices.data(),                   // pData_
        indices.size() * sizeof(uint16_t) // size_
    };

    // TODO: change to dynamic
    vra::ResourceId vertex_data_id = 0;
    vra::ResourceId index_data_id = 1;
    vra::ResourceId staging_vertex_data_id = 2;
    vra::ResourceId staging_index_data_id = 3;

    // vertex buffer create info
    VkBufferCreateInfo vertex_buffer_create_info = {};
    vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc vertex_data_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        vertex_buffer_create_info};

    // index buffer create info
    VkBufferCreateInfo index_buffer_create_info = {};
    index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc index_data_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        index_buffer_create_info};

    // staging buffer create info
    VkBufferCreateInfo staging_buffer_create_info = {};
    staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc staging_data_desc{
        vra::VraDataMemoryPattern::CPU_GPU,
        vra::VraDataUpdateRate::Frequent,
        staging_buffer_create_info};

    // collect and group
    vra_data_batcher_->Collect(vertex_data_desc, vertex_raw_data, vertex_data_id);
    vra_data_batcher_->Collect(index_data_desc, index_raw_data, index_data_id);
    vra_data_batcher_->Collect(staging_data_desc, vertex_raw_data, staging_vertex_data_id);
    vra_data_batcher_->Collect(staging_data_desc, index_raw_data, staging_index_data_id);
    vra_data_batcher_->Group();

    // generate vertex and index buffers and allocate memory
    auto group_data = vra_data_batcher_->GetGroupData("static_local_group");
    if (!group_data || group_data->offsets.size() == 0)
        throw std::runtime_error("Failed to get group data");
    const auto &local_buffer_create_info = group_data->data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags("static_local_group");
    if (vmaCreateBuffer(vma_allocator_, &local_buffer_create_info, &alloc_info, &local_buffer_, &local_buffer_allocation_, &local_buffer_allocation_info_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    // generate staging buffer and copy data
    auto staging_group_data = vra_data_batcher_->GetGroupData("dynamic_sequential_group");
    if (!staging_group_data || staging_group_data->offsets.size() == 0)
        throw std::runtime_error("Failed to get staging group data");
    const auto &host_buffer_create_info = staging_group_data->data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags("dynamic_sequential_group");
    if (vmaCreateBuffer(vma_allocator_, &host_buffer_create_info, &staging_alloc_info, &staging_buffer_, &staging_buffer_allocation_, &staging_buffer_allocation_info_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create staging buffer");

    // copy data to staging buffer
    void *mapped_data = nullptr;
    vmaMapMemory(vma_allocator_, staging_buffer_allocation_, &mapped_data);
    memcpy(mapped_data, staging_group_data->consolidated_data.data(), staging_group_data->consolidated_data.size());
    vmaUnmapMemory(vma_allocator_, staging_buffer_allocation_);

    // SVulkanCommandBufferAllocationConfig config;
    // config.command_buffer_count = 1;
    // config.command_buffer_level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // vkCommandBufferHelper_->AllocateCommandBuffer(config, "copy_command_buffer");

    // VkCommandBuffer command_buffer = vkCommandBufferHelper_->GetCommandBuffer("copy_command_buffer");
    // VkCommandBufferBeginInfo begin_info{};
    // begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // vkBeginCommandBuffer(command_buffer, &begin_info);

    // VkBufferCopy buffer_copy_info{};
    // buffer_copy_info.srcOffset = 0;
    // buffer_copy_info.dstOffset = 0;
    // buffer_copy_info.size = host_buffer_create_info.size;
    // vkCmdCopyBuffer(command_buffer, staging_buffer, buffer, 1, &buffer_copy_info);
    // vkEndCommandBuffer(command_buffer);

    // VkSubmitInfo submit_info{};
    // submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // submit_info.commandBufferCount = 1;
    // submit_info.pCommandBuffers = &command_buffer;
    // vkQueueSubmit(vkb_device_.get_queue(vkb::QueueType::graphics).value(), 1, &submit_info, VK_NULL_HANDLE);
    // vkQueueWaitIdle(vkb_device_.get_queue(vkb::QueueType::graphics).value());
}
