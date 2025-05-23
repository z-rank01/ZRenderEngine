#include "vulkan_engine.h"
#include <chrono>
#include <thread>
#include <iostream>

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
}

void VulkanEngine::Initialize()
{
    // initialize SDL, vulkan, and camera
    InitializeSDL();
    InitializeCamera();
    InitializeVulkan();

    // test gltf model
    // test_vulkan_gltf_model_ = std::make_unique<TestVulkanglTFModel>(vkb_device_.device, vkb_device_.get_queue(vkb::QueueType::graphics).value());
    // test_vulkan_gltf_model_->LoadGLTF("E:\\Assets\\Sponza\\SponzaBase\\NewSponza_Main_glTF_003.gltf");
}

void VulkanEngine::GetVertexIndexData(std::vector<gltf::PerDrawCallData> per_draw_call_data, std::vector<uint32_t> indices, std::vector<gltf::Vertex> vertices)
{
    per_draw_call_data_list_ = std::move(per_draw_call_data);
    indices_ = std::move(indices);
    vertices_ = std::move(vertices);
}

void VulkanEngine::GetMeshList(const std::vector<gltf::PerMeshData>& mesh_list)
{
    mesh_list_ = mesh_list;
}

VulkanEngine::~VulkanEngine()
{
    // release test data
    ReleaseTestData();

    // 等待设备空闲，确保没有正在进行的操作
    vkDeviceWaitIdle(vkb_device_.device);

    // 销毁深度资源
    if (depth_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(vkb_device_.device, depth_image_view_, nullptr);
        depth_image_view_ = VK_NULL_HANDLE;
    }
    
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(vkb_device_.device, depth_image_, nullptr);
        depth_image_ = VK_NULL_HANDLE;
    }
    
    if (depth_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(vkb_device_.device, depth_memory_, nullptr);
        depth_memory_ = VK_NULL_HANDLE;
    }
    
    // 销毁描述符相关资源
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vkb_device_.device, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vkb_device_.device, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    // 销毁VMA缓冲区
    // if (local_buffer_ != VK_NULL_HANDLE) {
    //     vmaDestroyBuffer(vma_allocator_, local_buffer_, local_buffer_allocation_);
    //     local_buffer_ = VK_NULL_HANDLE;
    // }

    // if (staging_buffer_ != VK_NULL_HANDLE) {
    //     vmaDestroyBuffer(vma_allocator_, staging_buffer_, staging_buffer_allocation_);
    //     staging_buffer_ = VK_NULL_HANDLE;
    // }

    if (uniform_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vma_allocator_, uniform_buffer_, uniform_buffer_allocation_);
        uniform_buffer_ = VK_NULL_HANDLE;
    }

    // 销毁VMA分配器
    if (vma_allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(vma_allocator_);
        vma_allocator_ = VK_NULL_HANDLE;
    }

    // destroy vkBootstrap swapchain
    vkb::destroy_swapchain(vkb_swapchain_);

    // release unique pointer
    vkShaderHelper_.reset();
    vkWindowHelper_.reset();
    vkRenderpassHelper_.reset();
    vkPipelineHelper_.reset();
    vkFrameBufferHelper_.reset();
    vkCommandBufferHelper_.reset();
    vkSynchronizationHelper_.reset();

    // destroy vkBootstrap resources
    vkb::destroy_device(vkb_device_);
    vkb::destroy_instance(vkb_instance_);

    // 重置单例指针
    instance = nullptr;
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

    if (!CreateVmaVraObjects())
    {
        throw std::runtime_error("Failed to create Vulkan vra and vma objects.");
    }

    // if (!CreateVertexInputBuffers())
    // {
    //     throw std::runtime_error("Failed to create Vulkan resource's buffers.");
    // }

    // test
    // CreateMeshListBuffer();
    CreateDrawCallListBuffer();

    if (!CreateUniformBuffers())
    {
        throw std::runtime_error("Failed to create Vulkan uniform buffers.");
    }

    if (!CreateAndWriteDescriptorRelatives())
    {
        throw std::runtime_error("Failed to create Vulkan descriptor relatives.");
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

void VulkanEngine::InitializeCamera()
{
    // initialize mvp matrices
    mvp_matrices_ = std::vector<SMvpMatrix>(engine_config_.frame_count, { glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f) });

    // initialize camera
    camera_.position = glm::vec3(0.0f, 0.0f, 10.0f);   // 更远的初始距离
    camera_.yaw = -90.0f;                              // look at origin
    camera_.pitch = 0.0f;                              // horizontal view
    camera_.wheel_speed = 0.1f;                        // 降低滚轮速度，避免变化太剧烈
    camera_.movement_speed = 5.0f;                     // 调整移动速度
    camera_.mouse_sensitivity = 0.2f;                  // 降低鼠标灵敏度
    camera_.zoom = 45.0f;
    camera_.world_up = glm::vec3(0.0f, 1.0f, 0.0f);    // Y-axis is up in Vulkan

    // initialize camera vectors
    camera_.front = glm::vec3(0.0f, 0.0f, -1.0f);     // look at -z direction
    camera_.right = glm::vec3(1.0f, 0.0f, 0.0f);      // right direction is +x
    camera_.up = glm::vec3(0.0f, 1.0f, 0.0f);         // up direction is +y (because Y-axis is up in Vulkan)

    // initialize focus point related parameters
    camera_.focus_point = glm::vec3(0.0f);            // default focus on origin
    camera_.has_focus_point = true;                   // default enable focus point
    camera_.focus_distance = 10.0f;                   // 增加默认焦距
    camera_.min_focus_distance = 0.5f;                // minimum focus distance
    camera_.max_focus_distance = 10000.0f;            // maximum focus distance
}

// Main loop
void VulkanEngine::Run()
{
    engine_state_ = EWindowState::Running;

    SDL_Event event;

    Uint64 last_time = SDL_GetTicks();
    float delta_time = 0.0f;

    // main loop
    while (engine_state_ != EWindowState::Stopped)
    {
        // calculate the time difference between frames
        Uint64 current_time = SDL_GetTicks();
        delta_time = (current_time - last_time) / 1000.0f; // convert to seconds
        last_time = current_time;

        // handle events on queue
        while (SDL_PollEvent(&event))
        {
            ProcessInput(event);

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

        // process keyboard input to update camera
        ProcessKeyboardInput(delta_time);

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

        // update the view matrix
        UpdateUniformBuffer(frame_index_);

        // render a frame
        Draw();
    }

    // wait until the GPU is completely idle before cleaning up
    vkDeviceWaitIdle(vkb_device_.device);
}

void VulkanEngine::ProcessInput(SDL_Event& event)
{
    // key down event
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        // ESC key to exit
        if (event.key.key == SDLK_ESCAPE)
        {
            engine_state_ = EWindowState::Stopped;
        }
        // Toggle focus constraint with 'F' key
        if (event.key.key == SDLK_F)
        {
            camera_.focus_constraint_enabled_ = !camera_.focus_constraint_enabled_;
            Logger::LogInfo(camera_.focus_constraint_enabled_ ? "Focus constraint enabled" : "Focus constraint disabled");
        }
    }

    // mouse button down event
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        float mouse_x, mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        last_x_ = mouse_x;
        last_y_ = mouse_y;

        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            // Use free_look_mode_ instead of camera_rotation_mode_
            free_look_mode_ = true;
            // save the current mouse position
            last_x_ = mouse_x;
            last_y_ = mouse_y;
        }
        else if (event.button.button == SDL_BUTTON_MIDDLE)
        {
            camera_pan_mode_ = true;
        }
    }

    // mouse button up event
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            // Use free_look_mode_ instead of camera_rotation_mode_
            free_look_mode_ = false;
        }
        else if (event.button.button == SDL_BUTTON_MIDDLE)
        {
            camera_pan_mode_ = false;
        }
    }

    // mouse motion event
    if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        // Only process mouse motion for camera control if a mode is active
        if (!free_look_mode_ && !camera_pan_mode_)
        {
            return; // Do nothing if no camera control mode is active
        }

        float x_pos = static_cast<float>(event.motion.x);
        float y_pos = static_cast<float>(event.motion.y);
        float x_offset = x_pos - last_x_;
        float y_offset = last_y_ - y_pos; // Invert y-offset as screen y increases downwards
        last_x_ = x_pos;
        last_y_ = y_pos;

        if (free_look_mode_)
        {
            // Calculate mouse sensitivity scale based on distance to focus point if constraint is enabled
            float sensitivity_scale = 1.0f;
            if (camera_.has_focus_point && camera_.focus_constraint_enabled_)
            {
                float current_distance = glm::length(camera_.position - camera_.focus_point);
                float distance_factor = glm::clamp(
                    current_distance / camera_.focus_distance,
                    camera_.min_focus_distance / camera_.focus_distance,
                    camera_.max_focus_distance / camera_.focus_distance
                );
                sensitivity_scale = distance_factor; // Slower when closer to focus point
            }

            // Apply sensitivity and scale
            float actual_x_offset = x_offset * camera_.mouse_sensitivity * sensitivity_scale;
            float actual_y_offset = y_offset * camera_.mouse_sensitivity * sensitivity_scale;

            // Update the yaw and pitch (Unity-style tracking mode)
            camera_.yaw += actual_x_offset;  // Changed from -= to +=
            camera_.pitch += actual_y_offset; // Changed from -= to +=

            // limit the pitch angle
            if (camera_.pitch > 89.0f) camera_.pitch = 89.0f;
            if (camera_.pitch < -89.0f) camera_.pitch = -89.0f;

            // calculate the new camera direction and update vectors
            camera_.UpdateCameraVectors();
        }

        if (camera_pan_mode_)
        {
            // calculate the current distance to the focus point
            float current_distance = camera_.has_focus_point ?
                glm::length(camera_.position - camera_.focus_point) :
                camera_.focus_distance;

            // calculate the distance scale based on the distance
            float distance_scale = glm::clamp(
                current_distance / camera_.focus_distance,
                camera_.min_focus_distance / camera_.focus_distance,
                camera_.max_focus_distance / camera_.focus_distance
            );

            float pan_speed_multiplier = 0.005f;
            // Apply distance scaling to pan speed if focus constraint is enabled
            float actual_pan_speed_multiplier = camera_.focus_constraint_enabled_ ? pan_speed_multiplier / distance_scale : pan_speed_multiplier;

            // calculate the target movement amount
            float target_x_offset = x_offset * camera_.movement_speed * actual_pan_speed_multiplier;
            float target_y_offset = y_offset * camera_.movement_speed * actual_pan_speed_multiplier;

            // Apply movement using camera's right and up vectors
            camera_.position -= camera_.right * target_x_offset; // Pan left/right
            camera_.position += camera_.up * target_y_offset;   // Pan up/down
        }
    }

    // mouse wheel event
    if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        float zoom_factor = camera_.wheel_speed;
        float distance = glm::length(camera_.position);

        if (event.wheel.y > 0)
        {
            if (distance > 0.5f)
            {
                camera_.position *= (1.0f - zoom_factor);
            }
        }
        else if (event.wheel.y < 0)
        {
            camera_.position *= (1.0f + zoom_factor);
        }

        ProcessMouseScroll(static_cast<float>(event.wheel.y));
    }
}

void VulkanEngine::ProcessKeyboardInput(float delta_time)
{
    // get the keyboard state
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);

    float velocity = camera_.movement_speed * delta_time;

    // If free look mode is enabled, move in world space based on camera orientation
    if (free_look_mode_)
    {
        // Calculate movement speed scale based on distance to focus point if constraint is enabled
        float distance_scale = 1.0f;
        if (camera_.has_focus_point && camera_.focus_constraint_enabled_)
        {
            float current_distance = glm::length(camera_.position - camera_.focus_point);
            distance_scale = glm::clamp(
                current_distance / camera_.focus_distance,
                camera_.min_focus_distance / camera_.focus_distance,
                camera_.max_focus_distance / camera_.focus_distance
            );
        }
        float current_velocity = velocity / distance_scale; // Slower when closer

        // move in the screen space
        glm::vec3 movement(0.0f);

        // move front/back (Z-axis relative to camera)
        if (keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP])
        {
            movement += camera_.front * current_velocity;
        }
        if (keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN])
        {
            movement -= camera_.front * current_velocity;
        }

        // move left/right (X-axis relative to camera)
        if (keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT])
        {
            movement -= camera_.right * current_velocity;
        }
        if (keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT])
        {
            movement += camera_.right * current_velocity;
        }

        // move up/down (Y-axis relative to world or camera up)
        if (keyboard_state[SDL_SCANCODE_Q])
        {
            movement += camera_.up * current_velocity; // Using camera's up vector for local up/down
        }
        if (keyboard_state[SDL_SCANCODE_E])
        {
            movement -= camera_.up * current_velocity; // Using camera's up vector for local up/down
        }

        // apply the movement
        camera_.position += movement;
    }
    else // Original screen space movement logic
    {
        // move in the screen space
        glm::vec3 movement(0.0f);

        // move up (Y-axis)
        if (keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP])
        {
            movement.y += velocity; // move up (Y-axis positive direction)
        }
        if (keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN])
        {
            movement.y -= velocity; // move down (Y-axis negative direction)
        }

        // move left (l-axis)t (X-axis)
        if (keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT])
        {
            movement.x -= velocity; // move left (l-axis negative direction)X-axis negative direction)
        }
        if (keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT])
        {
            movement.x += velocity; // move right (X-axis positive direction)
        }

        // move front (Z-axis)
        if (keyboard_state[SDL_SCANCODE_Q])
        {
            movement.z += velocity; // move back (Z-axis negative direction)
        }
        if (keyboard_state[SDL_SCANCODE_E])
        {
            movement.z -= velocity; // move front (Z-axis positive direction)
        }

        // apply the smooth movement (removed smooth factor for simplicity in free look, could add back if needed)
        // float smooth_factor = 0.1f; // smooth factor
        camera_.position += movement; // * smooth_factor;
    }
}

void VulkanEngine::ProcessMouseScroll(float yoffset)
{
    if (camera_.has_focus_point && camera_.focus_constraint_enabled_)
    {
        // Zoom by moving along the camera's front vector
        float zoom_step = camera_.movement_speed * 0.5f; // Adjust zoom speed

        // Calculate distance scale
        float current_distance = glm::length(camera_.position - camera_.focus_point);
        float distance_scale = glm::clamp(
            current_distance / camera_.focus_distance,
            camera_.min_focus_distance / camera_.focus_distance,
            camera_.max_focus_distance / camera_.focus_distance
        );
        zoom_step /= distance_scale; // Smaller steps when closer

        if (yoffset > 0)
        {
            // Zoom in (move towards focus point)
            camera_.position += camera_.front * zoom_step;
        }
        else if (yoffset < 0)
        {
            // Zoom out (move away from focus point)
            camera_.position -= camera_.front * zoom_step;
        }
        // Update camera vectors after changing position for orbit-like feel
        camera_.UpdateCameraVectors();
    }
    else
    {
        // Original FOV zoom logic when focus constraint is disabled
        camera_.zoom -= yoffset;
        if (camera_.zoom < 1.0f)
            camera_.zoom = 1.0f;
        if (camera_.zoom > 45.0f)
            camera_.zoom = 45.0f;
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
        output_frames_[i].image_available_semaphore_id = "image_available_semaphore_" + std::to_string(i);
        output_frames_[i].render_finished_semaphore_id = "render_finished_semaphore_" + std::to_string(i);
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
    VkPhysicalDeviceVulkan13Features features_13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features_13.synchronization2 = true;

    vkb::PhysicalDeviceSelector selector{ vkb_instance_ };
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
    vkb::DeviceBuilder device_builder{ vkb_physical_device_ };
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
    vkb::SwapchainBuilder swapchain_builder{ vkb_device_ };
    auto swap_ret = swapchain_builder
        .set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
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

bool VulkanEngine::CreateCommandPool()
{
    vkCommandBufferHelper_ = std::make_unique<VulkanCommandBufferHelper>();
    return vkCommandBufferHelper_->CreateCommandPool(vkb_device_.device, vkb_device_.get_queue_index(vkb::QueueType::graphics).value());
}

bool VulkanEngine::CreateVertexInputBuffers()
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec3 color;
    };

    // raw data
    const std::vector<Vertex> vertices =
        {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},    // 顶点 - 红色
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},     // 右下 - 绿色
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}     // 左下 - 蓝色
        };
    const std::vector<uint16_t> indices =
        {
            0, 1, 2  // 逆时针顺序
        };
    vra::VraRawData vertex_raw_data{
        vertices.data(),                 // pData_
        vertices.size() * sizeof(Vertex) // size_
    };
    vra::VraRawData index_raw_data{
        indices.data(),                   // pData_
        indices.size() * sizeof(uint16_t) // size_
    };

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
        vra::VraDataUpdateRate::RarelyOrNever,
        staging_buffer_create_info};

    // collect and group
    vra_data_batcher_->Collect(vertex_data_desc, vertex_raw_data, vertex_data_id_);
    vra_data_batcher_->Collect(index_data_desc, index_raw_data, index_data_id_);
    vra_data_batcher_->Collect(staging_data_desc, vertex_raw_data, staging_vertex_data_id_);
    vra_data_batcher_->Collect(staging_data_desc, index_raw_data, staging_index_data_id_);
    vertex_index_staging_batch_handle_ = std::move(vra_data_batcher_->Batch());

    // get buffer create info
    const auto& local_buffer_create_info = vertex_index_staging_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only].data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags(vra::VraDataMemoryPattern::GPU_Only, vra::VraDataUpdateRate::RarelyOrNever);
    if (!Logger::LogWithVkResult(vmaCreateBuffer(
                                     vma_allocator_,
                                     &local_buffer_create_info,
                                     &alloc_info,
                                     &local_buffer_,
                                     &local_buffer_allocation_,
                                     &local_buffer_allocation_info_),
                                 "Failed to create local buffer(From vma)",
                                 "Succeeded in creating local buffer(From vma)"))
        return false;


    // get buffer create info and consolidate data
    const auto& host_buffer_create_info = vertex_index_staging_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].data_desc.GetBufferCreateInfo();
    const auto& consolidated_data = vertex_index_staging_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].consolidated_data;

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags(vra::VraDataMemoryPattern::CPU_GPU, vra::VraDataUpdateRate::RarelyOrNever);
    if (!Logger::LogWithVkResult(vmaCreateBuffer(
                                     vma_allocator_,
                                     &host_buffer_create_info,
                                     &staging_alloc_info,
                                     &staging_buffer_,
                                     &staging_buffer_allocation_,
                                     &staging_buffer_allocation_info_),
                                 "Failed to create host buffer(From vma)",
                                 "Succeeded in creating host buffer(From vma)"))
        return false;

    // copy data to staging buffer
    void *mapped_data = nullptr;
    
    vmaInvalidateAllocation(vma_allocator_, staging_buffer_allocation_, 0, VK_WHOLE_SIZE);
    vmaMapMemory(vma_allocator_, staging_buffer_allocation_, &mapped_data);
    memcpy(mapped_data, consolidated_data.data(), consolidated_data.size());
    vmaUnmapMemory(vma_allocator_, staging_buffer_allocation_);
    vmaFlushAllocation(vma_allocator_, staging_buffer_allocation_, 0, VK_WHOLE_SIZE);

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

bool VulkanEngine::CreateUniformBuffers()
{
    for(int i = 0; i < engine_config_.frame_count; ++i)
    {
        auto current_mvp_matrix = mvp_matrices_[i];
        VkBufferCreateInfo buffer_create_info = {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_create_info.size = sizeof(SMvpMatrix);
        vra::VraDataDesc data_desc{
            vra::VraDataMemoryPattern::CPU_GPU,
            vra::VraDataUpdateRate::Frequent,
            buffer_create_info};
        vra::VraRawData raw_data{
            &current_mvp_matrix,
            sizeof(SMvpMatrix)};
        uniform_buffer_id_.push_back(0);
        vra_data_batcher_->Collect(data_desc, std::move(raw_data), uniform_buffer_id_.back());
    }

    uniform_batch_handle_ = std::move(vra_data_batcher_->Batch());

    // get buffer create info
    const auto& uniform_buffer_create_info = uniform_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Frequently].data_desc.GetBufferCreateInfo();

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags(vra::VraDataMemoryPattern::CPU_GPU, vra::VraDataUpdateRate::Frequent);
    allocation_create_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!Logger::LogWithVkResult(vmaCreateBuffer(
                                     vma_allocator_,
                                     &uniform_buffer_create_info,
                                     &allocation_create_info,
                                     &uniform_buffer_,
                                     &uniform_buffer_allocation_,
                                     &uniform_buffer_allocation_info_),
                                 "Failed to create uniform buffer",
                                 "Succeeded in creating uniform buffer"))
        return false;

    return true;
}

bool VulkanEngine::CreateAndWriteDescriptorRelatives()
{
    // create descriptor pool
    VkDescriptorType dynamic_uniform_buffer_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = dynamic_uniform_buffer_type;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    if (!Logger::LogWithVkResult(vkCreateDescriptorPool(vkb_device_.device, &pool_info, nullptr, &descriptor_pool_),
                                "Failed to create descriptor pool",
                                "Succeeded in creating descriptor pool"))
        return false;

    // create descriptor set layout

    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding = 0;
    layout_binding.descriptorType = dynamic_uniform_buffer_type;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &layout_binding;

    if (!Logger::LogWithVkResult(vkCreateDescriptorSetLayout(vkb_device_.device, &layout_info, nullptr, &descriptor_set_layout_),
                                "Failed to create descriptor set layout",
                                "Succeeded in creating descriptor set layout"))
        return false;

    // allocate descriptor set

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    if (!Logger::LogWithVkResult(vkAllocateDescriptorSets(vkb_device_.device, &alloc_info, &descriptor_set_),
                                "Failed to allocate descriptor set",
                                "Succeeded in allocating descriptor set"))
        return false;

    // write descriptor set
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffer_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(SMvpMatrix);

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = dynamic_uniform_buffer_type;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(vkb_device_.device, 1, &descriptor_write, 0, nullptr);

    return true;
}

bool VulkanEngine::CreateVmaVraObjects()
{
    // vra and vma members
    vra_data_batcher_ = std::make_unique<vra::VraDataBatcher>(vkb_physical_device_.physical_device);

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = vkb_physical_device_.physical_device;
    allocatorCreateInfo.device = vkb_device_.device;
    allocatorCreateInfo.instance = vkb_instance_.instance;

    if (!Logger::LogWithVkResult(vmaCreateAllocator(&allocatorCreateInfo, &vma_allocator_),
                                "Failed to create Vulkan vra and vma objects",
                                "Succeeded in creating Vulkan vra and vma objects"))
        return false;

    return true;
}

// 查找支持的深度格式
VkFormat VulkanEngine::FindSupportedDepthFormat()
{
    // 按优先级尝试不同的深度格式
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vkb_physical_device_.physical_device, format, &props);
        
        // 检查该格式是否支持作为深度附件的最佳平铺格式
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported depth format");
}

// 创建深度资源
bool VulkanEngine::CreateDepthResources()
{
    // 获取深度格式
    depth_format_ = FindSupportedDepthFormat();
    
    // 创建深度图像
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = swapchain_config_.target_swap_extent_.width;
    image_info.extent.height = swapchain_config_.target_swap_extent_.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = depth_format_;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // 创建图像
    if (vkCreateImage(vkb_device_.device, &image_info, nullptr, &depth_image_) != VK_SUCCESS) {
        Logger::LogError("Failed to create depth image");
        return false;
    }

    // 获取内存需求
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(vkb_device_.device, depth_image_, &mem_requirements);

    // 分配内存
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    
    // 查找适合的内存类型
    uint32_t memory_type_index = 0;
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(vkb_physical_device_.physical_device, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_requirements.memoryTypeBits & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memory_type_index = i;
            break;
        }
    }
    
    alloc_info.memoryTypeIndex = memory_type_index;

    // 分配内存
    if (vkAllocateMemory(vkb_device_.device, &alloc_info, nullptr, &depth_memory_) != VK_SUCCESS) {
        Logger::LogError("Failed to allocate depth image memory");
        return false;
    }

    // 绑定内存到图像
    if (vkBindImageMemory(vkb_device_.device, depth_image_, depth_memory_, 0) != VK_SUCCESS) {
        Logger::LogError("Failed to bind depth image memory");
        return false;
    }

    // 创建图像视图
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = depth_image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = depth_format_;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vkb_device_.device, &view_info, nullptr, &depth_image_view_) != VK_SUCCESS) {
        Logger::LogError("Failed to create depth image view");
        return false;
    }

    return true;
}

bool VulkanEngine::CreateFrameBuffer()
{
    // 创建深度资源
    if (!CreateDepthResources()) {
        throw std::runtime_error("Failed to create depth resources.");
    }
    
    // 创建帧缓冲
    auto swapchain_config = &swapchain_config_;
    auto swapchain_image_views = vkb_swapchain_.get_image_views().value();

    SVulkanFrameBufferConfig framebuffer_config(swapchain_config->target_swap_extent_, swapchain_image_views, depth_image_view_);

    vkFrameBufferHelper_ = std::make_unique<VulkanFrameBufferHelper>(vkb_device_.device, framebuffer_config);

    return vkFrameBufferHelper_->CreateFrameBuffer(vkRenderpassHelper_->GetRenderpass());
}

bool VulkanEngine::CreatePipeline()
{
    // create shader
    vkShaderHelper_ = std::make_unique<VulkanShaderHelper>(vkb_device_.device);

    std::vector<SVulkanShaderConfig> configs;
    std::string shader_path = engine_config_.general_config.working_directory + "src\\shader\\";
    // std::string vertex_shader_path = shader_path + "triangle.vert.spv";
    // std::string fragment_shader_path = shader_path + "triangle.frag.spv";
    std::string vertex_shader_path = shader_path + "gltf.vert.spv";
    std::string fragment_shader_path = shader_path + "gltf.frag.spv";
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
        {EShaderType::kFragmentShader, vkShaderHelper_->GetShaderModule(EShaderType::kFragmentShader)} };
    pipeline_config.renderpass = vkRenderpassHelper_->GetRenderpass();
    // pipeline_config.vertex_input_binding_description = vertex_input_binding_description_;
    // pipeline_config.vertex_input_attribute_descriptions = {vertex_input_attribute_position_, vertex_input_attribute_color_};
    pipeline_config.vertex_input_binding_description = test_vertex_input_binding_description_;
    pipeline_config.vertex_input_attribute_descriptions = test_vertex_input_attributes_;
    pipeline_config.descriptor_set_layouts.push_back(descriptor_set_layout_);
    vkPipelineHelper_ = std::make_unique<VulkanPipelineHelper>(pipeline_config);
    return vkPipelineHelper_->CreatePipeline(vkb_device_.device);
}

bool VulkanEngine::AllocatePerFrameCommandBuffer()
{
    for (int i = 0; i < engine_config_.frame_count; ++i)
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
    vkSynchronizationHelper_ = std::make_unique<VulkanSynchronizationHelper>(vkb_device_.device);
    // create synchronization objects
    for (int i = 0; i < engine_config_.frame_count; ++i)
    {
        if (!vkSynchronizationHelper_->CreateVkSemaphore(output_frames_[i].image_available_semaphore_id))
            return false;
        if (!vkSynchronizationHelper_->CreateVkSemaphore(output_frames_[i].render_finished_semaphore_id))
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
    auto current_image_available_semaphore_id = output_frames_[frame_index_].image_available_semaphore_id;
    auto current_render_finished_semaphore_id = output_frames_[frame_index_].render_finished_semaphore_id;
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
    vkb::destroy_swapchain(vkb_swapchain_);
    vkFrameBufferHelper_.reset();

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

    resize_request_ = false;
}

bool VulkanEngine::RecordCommand(uint32_t image_index, std::string command_buffer_id)
{
    // 更新当前帧的 Uniform Buffer
    UpdateUniformBuffer(image_index);

    // begin command recording
    if (!vkCommandBufferHelper_->BeginCommandBufferRecording(command_buffer_id, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        return false;

    // collect needed objects
    auto command_buffer = vkCommandBufferHelper_->GetCommandBuffer(command_buffer_id);
    auto swapchain_config = &swapchain_config_;

    // 从暂存缓冲区复制到本地缓冲区
    VkBufferCopy buffer_copy_info{};
    buffer_copy_info.srcOffset = 0;
    buffer_copy_info.dstOffset = 0;
    buffer_copy_info.size = test_staging_buffer_allocation_info_.size;
    vkCmdCopyBuffer(command_buffer, test_staging_buffer_, test_local_buffer_, 1, &buffer_copy_info);

    // 设置内存屏障以确保复制完成
    VkBufferMemoryBarrier2 buffer_memory_barrier{};
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    buffer_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    buffer_memory_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    buffer_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    buffer_memory_barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    buffer_memory_barrier.buffer = test_local_buffer_;
    buffer_memory_barrier.offset = 0;
    buffer_memory_barrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &buffer_memory_barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency_info);

    // begin renderpass
    VkClearValue clear_color = {};
    clear_color.color.float32[0] = 0.1f;
    clear_color.color.float32[1] = 0.1f;
    clear_color.color.float32[2] = 0.1f;
    clear_color.color.float32[3] = 1.0f;

    VkClearValue clear_values[2];
    clear_values[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0}; // 设置深度清除值为1.0（远面）

    VkRenderPassBeginInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = vkRenderpassHelper_->GetRenderpass();
    renderpass_info.framebuffer = (*vkFrameBufferHelper_->GetFramebuffers())[image_index];
    renderpass_info.renderArea.offset = {0, 0};
    renderpass_info.renderArea.extent = swapchain_config->target_swap_extent_;
    renderpass_info.clearValueCount = 2;
    renderpass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

    // bind pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineHelper_->GetPipeline());

    // bind descriptor set
    auto offset = uniform_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Frequently].offsets[uniform_buffer_id_[frame_index_]];
    uint32_t dynamic_offset = static_cast<uint32_t>(offset);
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vkPipelineHelper_->GetPipelineLayout(),
        0,
        1,
        &descriptor_set_,
        1,
        &dynamic_offset);

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
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_config->target_swap_extent_;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // 绑定顶点和索引缓冲区
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &test_local_buffer_, &test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only].offsets[test_vertex_buffer_id_]);
    vkCmdBindIndexBuffer(command_buffer, test_local_buffer_, test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only].offsets[test_index_buffer_id_], VK_INDEX_TYPE_UINT32);

    // 遍历每个 mesh 进行绘制
    for (const auto& mesh : mesh_list_) {
        // 遍历该 mesh 的所有图元进行绘制
        // if (std::strcmp(mesh.name.c_str(), "arch_stones_02") != 0) {
        //     continue;
        // }
        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            const auto& primitive = mesh.primitives[i];

            // // 获取该 primitive 的顶点和索引缓冲区偏移量
            // VkDeviceSize vertex_offset = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only]
            //     .offsets[mesh_vertex_resource_ids_[mesh.name][i]];
            // VkDeviceSize index_offset = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only]
            //     .offsets[mesh_index_resource_ids_[mesh.name][i]];

            // 绘制当前图元
            vkCmdDrawIndexed(
                command_buffer,
                primitive.index_count,  // 使用实际的索引数量
                1,
                primitive.first_index, // 使用实际的索引偏移量
                0,
                0
            );
        }
    }

    // end renderpass
    vkCmdEndRenderPass(command_buffer);

    // end command recording
    if (!vkCommandBufferHelper_->EndCommandBufferRecording(command_buffer_id))
        return false;

    return true;
}

void VulkanEngine::UpdateUniformBuffer(uint32_t current_frame_index)
{
    // update the model matrix (添加适当的缩放)
    mvp_matrices_[current_frame_index].model = glm::mat4(1.0f);

    // update the view matrix
    mvp_matrices_[current_frame_index].view = glm::lookAt(
        camera_.position,                       // camera position
        camera_.position + camera_.front,    // camera looking at point
        camera_.up                                   // camera up direction
    );

    // update the projection matrix
    mvp_matrices_[current_frame_index].projection = glm::perspective(
        glm::radians(camera_.zoom),            // FOV
        swapchain_config_.target_swap_extent_.width / (float)swapchain_config_.target_swap_extent_.height, // aspect ratio
        0.1f,                                  // near plane
        1000.0f                                // 增加远平面距离，确保能看到远处的物体
    );

    // reverse the Y-axis in Vulkan's NDC coordinate system
    mvp_matrices_[current_frame_index].projection[1][1] *= -1;

    // map vulkan host memory to update the uniform buffer
    uniform_buffer_mapped_data_ = nullptr;
    vmaMapMemory(vma_allocator_, uniform_buffer_allocation_, &uniform_buffer_mapped_data_);

    // get the offset of the current frame in the uniform buffer
    auto offset = uniform_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Frequently].offsets[uniform_buffer_id_[current_frame_index]];
    uint8_t* data_location = static_cast<uint8_t*>(uniform_buffer_mapped_data_) + offset;

    // copy the data to the mapped memory
    memcpy(data_location, &mvp_matrices_[current_frame_index], sizeof(SMvpMatrix));

    // unmap the memory
    vmaUnmapMemory(vma_allocator_, uniform_buffer_allocation_);
    uniform_buffer_mapped_data_ = nullptr;
}

// add a function to focus on an object
void VulkanEngine::FocusOnObject(const glm::vec3& object_position, float target_distance)
{
    camera_.focus_point = object_position;
    camera_.has_focus_point = true;

    // calculate the position the camera should move to
    glm::vec3 direction = glm::normalize(camera_.position - object_position);
    camera_.position = object_position + direction * target_distance;

    // update the camera direction
    camera_.front = glm::normalize(object_position - camera_.position);
    camera_.right = glm::normalize(glm::cross(camera_.front, camera_.world_up));
    camera_.up = glm::normalize(glm::cross(camera_.right, camera_.front));

    // update the yaw and pitch
    glm::vec3 front = camera_.front;
    camera_.pitch = glm::degrees(asin(front.y));
    camera_.yaw = glm::degrees(atan2(front.z, front.x));
}

void VulkanEngine::CreateMeshListBuffer()
{
    // 为每个 mesh 收集数据
    for (const auto& mesh : mesh_list_) {
        // 遍历每个 primitive
        for (const auto& primitive : mesh.primitives) {
            // 直接使用 primitive 中的数据
            vra::VraRawData vertex_buffer_data{
                .pData_ = primitive.vertices.data(),
                .size_ = sizeof(gltf::Vertex) * primitive.vertices.size()
            };
            vra::VraRawData index_buffer_data{
                .pData_ = primitive.indices.data(),
                .size_ = sizeof(uint32_t) * primitive.indices.size()
            };

            // 顶点缓冲区创建信息
            VkBufferCreateInfo vertex_buffer_create_info{};
            vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            vertex_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vertex_buffer_create_info.size = vertex_buffer_data.size_;
            vra::VraDataDesc vertex_buffer_desc{
                vra::VraDataMemoryPattern::GPU_Only,
                vra::VraDataUpdateRate::RarelyOrNever,
                vertex_buffer_create_info
            };

            // 索引缓冲区创建信息
            VkBufferCreateInfo index_buffer_create_info{};
            index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            index_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            index_buffer_create_info.size = index_buffer_data.size_;
            vra::VraDataDesc index_buffer_desc{
                vra::VraDataMemoryPattern::GPU_Only,
                vra::VraDataUpdateRate::RarelyOrNever,
                index_buffer_create_info
            };

            // 暂存缓冲区创建信息
            VkBufferCreateInfo staging_buffer_create_info{};
            staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vra::VraDataDesc staging_vertex_buffer_desc{
                vra::VraDataMemoryPattern::CPU_GPU,
                vra::VraDataUpdateRate::RarelyOrNever,
                staging_buffer_create_info
            };

            staging_buffer_create_info.size = index_buffer_data.size_;  // 索引数据的暂存缓冲区
            vra::VraDataDesc staging_index_buffer_desc{
                vra::VraDataMemoryPattern::CPU_GPU,
                vra::VraDataUpdateRate::RarelyOrNever,
                staging_buffer_create_info
            };

            // 收集数据并获取资源 ID
            vra::ResourceId vertex_id = 0;
            vra::ResourceId index_id = 0;
            vra::ResourceId staging_vertex_id = 0;
            vra::ResourceId staging_index_id = 0;

            if (!vra_data_batcher_->Collect(vertex_buffer_desc, vertex_buffer_data, vertex_id)) {
                Logger::LogError("Failed to collect vertex buffer data for mesh: " + mesh.name);
                continue;
            }
            if (!vra_data_batcher_->Collect(index_buffer_desc, index_buffer_data, index_id)) {
                Logger::LogError("Failed to collect index buffer data for mesh: " + mesh.name);
                continue;
            }
            if (!vra_data_batcher_->Collect(staging_vertex_buffer_desc, vertex_buffer_data, staging_vertex_id)) {
                Logger::LogError("Failed to collect staging vertex buffer data for mesh: " + mesh.name);
                continue;
            }
            if (!vra_data_batcher_->Collect(staging_index_buffer_desc, index_buffer_data, staging_index_id)) {
                Logger::LogError("Failed to collect staging index buffer data for mesh: " + mesh.name);
                continue;
            }

            // 存储资源 ID
            if (mesh_vertex_resource_ids_[mesh.name].empty()) {
                mesh_vertex_resource_ids_[mesh.name].reserve(mesh.primitives.size());
                mesh_index_resource_ids_[mesh.name].reserve(mesh.primitives.size());
            }
            mesh_vertex_resource_ids_[mesh.name].push_back(vertex_id);
            mesh_index_resource_ids_[mesh.name].push_back(index_id);
        }
    }

    // 执行批处理
    test_local_host_batch_handle_ = std::move(vra_data_batcher_->Batch());

    // 创建本地缓冲区
    auto test_local_buffer_create_info = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only].data_desc.GetBufferCreateInfo();
    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateBuffer(
        vma_allocator_,
        &test_local_buffer_create_info,
        &allocation_create_info,
        &test_local_buffer_,
        &test_local_buffer_allocation_,
        &test_local_buffer_allocation_info_);

    // 创建暂存缓冲区
    auto test_host_buffer_create_info = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].data_desc.GetBufferCreateInfo();
    VmaAllocationCreateInfo staging_allocation_create_info{};
    staging_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_allocation_create_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags(vra::VraDataMemoryPattern::CPU_GPU, vra::VraDataUpdateRate::RarelyOrNever);
    vmaCreateBuffer(
        vma_allocator_,
        &test_host_buffer_create_info,
        &staging_allocation_create_info,
        &test_staging_buffer_,
        &test_staging_buffer_allocation_,
        &test_staging_buffer_allocation_info_);

    // 复制数据到暂存缓冲区
    auto consolidate_data = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].consolidated_data;
    void* data;
    vmaInvalidateAllocation(vma_allocator_, test_staging_buffer_allocation_, 0, VK_WHOLE_SIZE);
    vmaMapMemory(vma_allocator_, test_staging_buffer_allocation_, &data);
    memcpy(data, consolidate_data.data(), consolidate_data.size());
    vmaUnmapMemory(vma_allocator_, test_staging_buffer_allocation_);
    vmaFlushAllocation(vma_allocator_, test_staging_buffer_allocation_, 0, VK_WHOLE_SIZE);

    // 设置顶点输入绑定描述
    test_vertex_input_binding_description_.binding = 0;
    test_vertex_input_binding_description_.stride = sizeof(gltf::Vertex);
    test_vertex_input_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // 设置顶点属性描述
    test_vertex_input_attributes_.clear();

    // 使用更安全的偏移量计算，确保 offsetof 计算正确
    // position
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(gltf::Vertex, position)
        });
    // color
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(gltf::Vertex, color)
        });
    // normal
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(gltf::Vertex, normal)
        });
    // tangent
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(gltf::Vertex, tangent)
        });
    // uv0
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 4,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(gltf::Vertex, uv0)
        });
    // uv1
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 5,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(gltf::Vertex, uv1)
        });
}

void VulkanEngine::CreateDrawCallListBuffer()
{
    vra::VraRawData vertex_buffer_data{
        .pData_ = vertices_.data(),
        .size_ = sizeof(gltf::Vertex) * vertices_.size()
    };
    vra::VraRawData index_buffer_data{
        .pData_ = indices_.data(),
        .size_ = sizeof(uint32_t) * indices_.size()
    };

    // 顶点缓冲区创建信息
    VkBufferCreateInfo vertex_buffer_create_info{};
    vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc vertex_buffer_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        vertex_buffer_create_info
    };

    // 索引缓冲区创建信息
    VkBufferCreateInfo index_buffer_create_info{};
    index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc index_buffer_desc{
        vra::VraDataMemoryPattern::GPU_Only,
        vra::VraDataUpdateRate::RarelyOrNever,
        index_buffer_create_info
    };

    // 暂存缓冲区创建信息
    VkBufferCreateInfo staging_buffer_create_info{};
    staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vra::VraDataDesc staging_vertex_buffer_desc{
        vra::VraDataMemoryPattern::CPU_GPU,
        vra::VraDataUpdateRate::RarelyOrNever,
        staging_buffer_create_info
    };
    vra::VraDataDesc staging_index_buffer_desc{
        vra::VraDataMemoryPattern::CPU_GPU,
        vra::VraDataUpdateRate::RarelyOrNever,
        staging_buffer_create_info
    };

    if (!vra_data_batcher_->Collect(vertex_buffer_desc, vertex_buffer_data, test_vertex_buffer_id_)) {
        Logger::LogError("Failed to collect vertex buffer data");
        return;
    }
    if (!vra_data_batcher_->Collect(index_buffer_desc, index_buffer_data, test_index_buffer_id_)) {
        Logger::LogError("Failed to collect index buffer data");
        return;
    }
    if (!vra_data_batcher_->Collect(staging_vertex_buffer_desc, vertex_buffer_data, test_staging_vertex_buffer_id_)) {
        Logger::LogError("Failed to collect staging vertex buffer data");
        return;
    }
    if (!vra_data_batcher_->Collect(staging_index_buffer_desc, index_buffer_data, test_staging_index_buffer_id_)) {
        Logger::LogError("Failed to collect staging index buffer data");
        return;
    }

    // 执行批处理
    test_local_host_batch_handle_ = std::move(vra_data_batcher_->Batch());

    // 创建本地缓冲区
    auto test_local_buffer_create_info = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::GPU_Only].data_desc.GetBufferCreateInfo();
    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateBuffer(
        vma_allocator_,
        &test_local_buffer_create_info,
        &allocation_create_info,
        &test_local_buffer_,
        &test_local_buffer_allocation_,
        &test_local_buffer_allocation_info_);

    // 创建暂存缓冲区
    auto test_host_buffer_create_info = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].data_desc.GetBufferCreateInfo();
    VmaAllocationCreateInfo staging_allocation_create_info{};
    staging_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_allocation_create_info.flags = vra_data_batcher_->GetSuggestVmaMemoryFlags(vra::VraDataMemoryPattern::CPU_GPU, vra::VraDataUpdateRate::RarelyOrNever);
    vmaCreateBuffer(
        vma_allocator_,
        &test_host_buffer_create_info,
        &staging_allocation_create_info,
        &test_staging_buffer_,
        &test_staging_buffer_allocation_,
        &test_staging_buffer_allocation_info_);

    // 复制数据到暂存缓冲区
    auto consolidate_data = test_local_host_batch_handle_[vra::VraBuiltInBatchIds::CPU_GPU_Rarely].consolidated_data;
    void* data;
    vmaInvalidateAllocation(vma_allocator_, test_staging_buffer_allocation_, 0, VK_WHOLE_SIZE);
    vmaMapMemory(vma_allocator_, test_staging_buffer_allocation_, &data);
    memcpy(data, consolidate_data.data(), consolidate_data.size());
    vmaUnmapMemory(vma_allocator_, test_staging_buffer_allocation_);
    vmaFlushAllocation(vma_allocator_, test_staging_buffer_allocation_, 0, VK_WHOLE_SIZE);

    // 设置顶点输入绑定描述
    test_vertex_input_binding_description_.binding = 0;
    test_vertex_input_binding_description_.stride = sizeof(gltf::Vertex);
    test_vertex_input_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // 设置顶点属性描述
    test_vertex_input_attributes_.clear();

    // 使用更安全的偏移量计算，确保 offsetof 计算正确
    // position
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(gltf::Vertex, position)
        });
    // color
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(gltf::Vertex, color)
        });
    // normal
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(gltf::Vertex, normal)
        });
    // tangent
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(gltf::Vertex, tangent)
        });
    // uv0
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 4,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(gltf::Vertex, uv0)
        });
    // uv1
    test_vertex_input_attributes_.push_back(VkVertexInputAttributeDescription{
        .location = 5,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(gltf::Vertex, uv1)
        });
}

void VulkanEngine::ReleaseTestData()
{
    // release vma
    vmaDestroyBuffer(vma_allocator_, test_local_buffer_, test_local_buffer_allocation_);
    vmaDestroyBuffer(vma_allocator_, test_staging_buffer_, test_staging_buffer_allocation_);
    // vmaDestroyBuffer(vma_allocator_, test_uniform_buffer_, test_uniform_buffer_allocation_);

    // // release descriptor set
    // vkDestroyDescriptorPool(vkb_device_.device, test_descriptor_pool_, nullptr);
    // vkDestroyDescriptorSetLayout(vkb_device_.device, test_descriptor_set_layout_, nullptr); 
}