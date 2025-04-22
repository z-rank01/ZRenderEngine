#pragma once

#include <vulkan/vulkan.h>
#include <map>
#include "utility/logger.h"

struct SVulkanCommandBufferAllocationConfig
{
    VkCommandBufferLevel command_buffer_level;
    uint32_t command_buffer_count;
};

class VulkanCommandBufferHelper
{
private:
    VkCommandPool command_pool_;
    std::map<std::string, VkCommandBuffer> command_buffer_map_;
    VkDevice device_;
public:
    VulkanCommandBufferHelper();
    ~VulkanCommandBufferHelper();

    VkCommandBuffer GetCommandBuffer(std::string id) const;
    bool CreateCommandPool(VkDevice device, uint32_t queue_family_index);
    bool AllocateCommandBuffer(const SVulkanCommandBufferAllocationConfig& config, std::string id);
    bool BeginCommandBufferRecording(std::string id, VkCommandBufferUsageFlags usage_flags);
    bool EndCommandBufferRecording(std::string id);
    bool ResetCommandBuffer(std::string id);
};