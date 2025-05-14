#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "utility/logger.h"

struct SVulkanFrameBufferConfig
{
    VkExtent2D extent;
    std::vector<VkImageView> image_views;

    SVulkanFrameBufferConfig() = default;
    SVulkanFrameBufferConfig(VkExtent2D extent, std::vector<VkImageView> image_views) 
    {
        this->extent = extent;
        this->image_views = image_views;
    }
};

class VulkanFrameBufferHelper
{
private:
    VkDevice device_;
    SVulkanFrameBufferConfig config_;
    std::vector<VkFramebuffer> framebuffers_;

public:
    VulkanFrameBufferHelper(VkDevice device, SVulkanFrameBufferConfig config) : device_(device), config_(config) {};
    ~VulkanFrameBufferHelper();

    bool CreateFrameBuffer(VkRenderPass renderpass);
    const std::vector<VkFramebuffer>* GetFramebuffers() const { return &framebuffers_; }
};
