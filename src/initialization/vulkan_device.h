#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct SVulkanDeviceConfig
{

};

class VulkanDeviceHelper
{
public:
    VulkanDeviceHelper() = delete;
    VulkanDeviceHelper(SVulkanDeviceConfig config);
    ~VulkanDeviceHelper();

    bool CreateLogicalDevice(VkDevice& device);

private:
    SVulkanDeviceConfig device_config_;
};