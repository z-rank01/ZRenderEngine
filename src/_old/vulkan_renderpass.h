#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "utility/logger.h"

struct SVulkanRenderpassConfig
{
    VkFormat color_format;
    VkFormat depth_format;
    VkSampleCountFlagBits sample_count;
};

class VulkanRenderpassHelper
{
private:
    SVulkanRenderpassConfig config_;
    VkRenderPass renderpass_;
    VkDevice device_;
public:
    VulkanRenderpassHelper() = delete;
    VulkanRenderpassHelper(SVulkanRenderpassConfig config) : config_(config) {};
    ~VulkanRenderpassHelper();

    bool CreateRenderpass(VkDevice device);
    VkRenderPass GetRenderpass() const { return renderpass_; }
};
