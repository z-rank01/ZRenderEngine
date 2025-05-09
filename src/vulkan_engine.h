#pragma once

#include "initialization/vulkan_instance.h"
#include "initialization/vulkan_device.h"
#include "initialization/vulkan_queue.h"
#include "initialization/vulkan_window.h"

#include "pipeline/vulkan_shader.h"
#include "pipeline/vulkan_pipeline.h"
#include "pipeline/vulkan_renderpass.h"

#include "source/vulkan_commandbuffer.h"
#include "source/vulkan_framebuffer.h"

#include "synchronization/vulkan_synchronization.h"
#include "utility/config_reader.h"

#include "vulkan_resource_allocator/vra.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>
#include <VkBootstrap.h>
#include <glm/glm.hpp>

enum class EWindowState : std::uint8_t
{
    Initialized, // Engine is initialized but not running
    Running,     // Engine is running
    Stopped      // Engine is stopped
};

enum class ERenderState : std::uint8_t
{
    True,   // Render is enabled 
    False   // Render is disabled
};

struct SWindowConfig
{
    int width;
    int height;
    std::string title;

    [[nodiscard]] constexpr auto Validate() const -> bool
    {
        return width > 0 && height > 0;
    }
};

struct SEngineConfig
{
    SWindowConfig window_config;
    SGeneralConfig general_config;
    uint8_t frame_count;
    bool use_validation_layers;
};

struct SOutputFrame
{
    uint32_t image_index;
    std::string queue_id;
    std::string command_buffer_id;
    std::string image_available_sempaphore_id;
    std::string render_finished_sempaphore_id;
    std::string fence_id;
};

struct SMvpMatrix
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

class VulkanEngine
{
public:
    VulkanEngine() = delete;
    VulkanEngine(const SEngineConfig& config);
    ~VulkanEngine();
    
    void Run();
    void Draw();

    static VulkanEngine& GetInstance();

private:
    // engine members
    uint8_t frame_index_ = 0;
    bool resize_request_ = false;
    EWindowState engine_state_;
    ERenderState render_state_;
    SEngineConfig engine_config_;
    std::vector<SOutputFrame> output_frames_;

    // vulkan bootstrap members
    vkb::Instance vkb_instance_;
    vkb::PhysicalDevice vkb_physical_device_;
    vkb::Device vkb_device_;
    vkb::Swapchain vkb_swapchain_;

    SVulkanSwapChainConfig swapchain_config_;

    // vra and vma members
    VmaAllocator vma_allocator_;
    VmaAllocation local_buffer_allocation_;
    VmaAllocation staging_buffer_allocation_;
    std::vector<VmaAllocation> uniform_buffer_allocation_;
    VmaAllocationInfo local_buffer_allocation_info_;
    VmaAllocationInfo staging_buffer_allocation_info_;
    std::vector<VmaAllocationInfo> uniform_buffer_allocation_info_;
    std::unique_ptr<vra::VraDataBatcher> vra_data_batcher_;

    // TODO: change to dynamic
    vra::ResourceId vertex_data_id_ = 0;
    vra::ResourceId index_data_id_ = 1;
    vra::ResourceId staging_vertex_data_id_ = 2;
    vra::ResourceId staging_index_data_id_ = 3;
    vra::ResourceId uniform_buffer_id_ = 4;


    // vulkan native members
    VkBuffer local_buffer_;
    VkBuffer staging_buffer_;
    std::vector<VkBuffer> uniform_buffer_;
    VkDescriptorPool descriptor_pool_;
    VkDescriptorSetLayout descriptor_set_layout_;
    VkDescriptorSet descriptor_set_;
    VkVertexInputBindingDescription vertex_input_binding_description_;
    VkVertexInputAttributeDescription vertex_input_attribute_position_;
    VkVertexInputAttributeDescription vertex_input_attribute_color_;

    // vulkan helper members
    std::unique_ptr<VulkanSDLWindowHelper> vkWindowHelper_;
    std::unique_ptr<VulkanShaderHelper> vkShaderHelper_;
    std::unique_ptr<VulkanRenderpassHelper> vkRenderpassHelper_;
    std::unique_ptr<VulkanPipelineHelper> vkPipelineHelper_;
    std::unique_ptr<VulkanCommandBufferHelper> vkCommandBufferHelper_;
    std::unique_ptr<VulkanFrameBufferHelper> vkFrameBufferHelper_;
    std::unique_ptr<VulkanSynchronizationHelper> vkSynchronizationHelper_;
    // uniform data
    std::vector<SMvpMatrix> mvp_matrices_;
    std::vector<void *> uniform_buffer_mapped_data_;
    
    void InitializeSDL();
    void InitializeVulkan();

    // --- Vulkan Initialization Steps ---
    void GenerateFrameStructs();
    bool CreateInstance();
    bool CreateSurface();
    bool CreatePhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapChain();
    bool CreatePipeline();
    bool CreateFrameBuffer();
    bool CreateCommandPool();
    bool CreateDescriptorRelatives();
    bool CreateVertexInputBuffers();
    bool CreateUniformBuffers();
    bool AllocatePerFrameCommandBuffer();
    bool CreateSynchronizationObjects();
    // ------------------------------------

    // --- Vulkan Draw Steps ---
    void DrawFrame();
    void ResizeSwapChain();
    bool RecordCommand(uint32_t image_index, std::string command_buffer_id);
    // --------------------------------------

    // --- VRA Test Functions ---
    void TestVraFunctions();
    // --------------------------
};
