#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "utility/logger.h"

struct SVulkanFrameBufferConfig
{
    VkExtent2D extent;
    const std::vector<VkImageView>* image_views;
};

class VulkanFrameBufferHelper
{
private:
    SVulkanFrameBufferConfig config_;
    std::vector<VkFramebuffer> framebuffers_;
    VkDevice device_;
public:
    VulkanFrameBufferHelper(SVulkanFrameBufferConfig config) : config_(config) {};
    ~VulkanFrameBufferHelper();

    bool CreateFrameBuffer(VkDevice device, VkRenderPass renderpass);
    const std::vector<VkFramebuffer>* GetFramebuffers() const { return &framebuffers_; }
};
