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
#include "vulkan_structure.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>

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

    // vulkan helper members
    std::unique_ptr<VulkanSDLWindowHelper> vkWindowHelper_;
    std::unique_ptr<VulkanInstanceHelper> vkInstanceHelper_;
    std::unique_ptr<VulkanDeviceHelper> vkDeviceHelper_;
    std::unique_ptr<VulkanQueueHelper> vkQueueHelper_;
    std::unique_ptr<VulkanSwapChainHelper> vkSwapChainHelper_;
    std::unique_ptr<VulkanShaderHelper> vkShaderHelper_;
    std::unique_ptr<VulkanRenderpassHelper> vkRenderpassHelper_;
    std::unique_ptr<VulkanPipelineHelper> vkPipelineHelper_;
    std::unique_ptr<VulkanCommandBufferHelper> vkCommandBufferHelper_;
    std::unique_ptr<VulkanFrameBufferHelper> vkFrameBufferHelper_;
    std::unique_ptr<VulkanSynchronizationHelper> vkSynchronizationHelper_;
    
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
    bool AllocateCommandBuffer();
    bool CreateSynchronizationObjects();
    // ------------------------------------

    // --- Vulkan Draw Steps ---
    void DrawFrame();
    void ResizeSwapChain();
    bool RecordCommand(uint32_t image_index, std::string command_buffer_id);
    // --------------------------------------
};
