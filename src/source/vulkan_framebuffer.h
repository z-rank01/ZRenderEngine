#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../utility/logger.h"

// 帧缓冲配置结构体
struct SVulkanFrameBufferConfig
{
    VkExtent2D extent_;
    std::vector<VkImageView> swapchain_image_views_;
    VkImageView depth_image_view_ = VK_NULL_HANDLE; // 添加深度图像视图字段

    SVulkanFrameBufferConfig() = default;
    SVulkanFrameBufferConfig(VkExtent2D extent, const std::vector<VkImageView> swapchain_image_views, VkImageView depth_image_view = VK_NULL_HANDLE)
        : extent_(extent), swapchain_image_views_(swapchain_image_views), depth_image_view_(depth_image_view) {}
};

class VulkanFrameBufferHelper
{
public:
    VulkanFrameBufferHelper() = delete;
    VulkanFrameBufferHelper(VkDevice device, const SVulkanFrameBufferConfig& config)
        : device_(device), config_(config) {}
    ~VulkanFrameBufferHelper();

    bool CreateFrameBuffer(VkRenderPass renderpass);
    [[nodiscard]] constexpr auto GetFramebuffers() const -> const std::vector<VkFramebuffer>* { return &framebuffers_; }

private:
    VkDevice device_;
    SVulkanFrameBufferConfig config_;
    std::vector<VkFramebuffer> framebuffers_;
};
