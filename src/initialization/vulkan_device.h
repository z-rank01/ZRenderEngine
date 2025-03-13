#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct SVulkanDeviceConfig
{
    // physical device config
    VkPhysicalDeviceType physical_device_type;
    uint8_t physical_device_api_version[4];
    std::vector<VkBool32> physical_device_features;
    
    // Queue and Queue Family config
    std::vector<VkQueueFlagBits> queue_flags;
};

class VulkanDeviceHelper
{
public:
    VulkanDeviceHelper();
    VulkanDeviceHelper(SVulkanDeviceConfig config);
    ~VulkanDeviceHelper();

    bool CreateLogicalDevice();
    bool CreatePhysicalDevice(const VkInstance& instance);
    bool CreateQueue();

private:
    SVulkanDeviceConfig device_config_;
    VkPhysicalDevice vkPhysicalDevice_;
    VkDevice vkLogicalDevice_;

    const VkPhysicalDevice& PickPhysicalDevice(const std::vector<VkPhysicalDevice>& instance);
    uint32_t PickQueueFamily(const VkPhysicalDevice& physical_device);
};