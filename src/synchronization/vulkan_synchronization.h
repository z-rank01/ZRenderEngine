#pragma once

#include <vulkan/vulkan.h>
#include <map>
#include "utility/logger.h"

class VulkanSynchronizationHelper
{
private:
    VkDevice device_;
    std::map<std::string, VkSemaphore> semaphores_;
    std::map<std::string, VkFence> fences_;
public:
    VulkanSynchronizationHelper(VkDevice device) : device_(device) {};
    ~VulkanSynchronizationHelper();

    bool CreateSemaphore(std::string id);
    bool CreateFence(std::string id);

    bool WaitForFence(std::string id);
    bool ResetFence(std::string id);

    VkSemaphore GetSemaphore(std::string id) const;
    VkFence GetFence(std::string id) const;
};
