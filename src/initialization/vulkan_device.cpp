#pragma once

#include "vulkan_device.h"
#include <utility/logger.h>

///-------------------------------------------------------------------------------------------------
// helper function declaration
int CountSupportedPropertiesOrFeatures(const SVulkanDeviceConfig& device_config, const VkPhysicalDevice& physical_device);


VulkanDeviceHelper::VulkanDeviceHelper()
{
}

///-------------------------------------------------------------------------------------------------
// VulkanDeviceHelper implementation
VulkanDeviceHelper::VulkanDeviceHelper(SVulkanDeviceConfig config)
    : device_config_(config)
{
}

VulkanDeviceHelper::~VulkanDeviceHelper()
{
    vkDestroyDevice(vkLogicalDevice_, nullptr);
}

bool VulkanDeviceHelper::CreateLogicalDevice(const VkDeviceQueueCreateInfo& queue_create_info)
{
    // create logical device
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_create_info;
    device_info.enabledExtensionCount = 0;
    device_info.ppEnabledExtensionNames = nullptr;
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = nullptr;
    device_info.pEnabledFeatures = nullptr;
    if (!Logger::LogWithVkResult(
        vkCreateDevice(vkPhysicalDevice_, &device_info, nullptr, &vkLogicalDevice_),
        "Failed to create logical device",
        "Succeeded in creating logical device"))
        return false;
    else
        return true;
}

bool VulkanDeviceHelper::CreatePhysicalDevice(const VkInstance& instance)
{
    // enumerate physical devices
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        Logger::LogError("Failed to find GPUs with Vulkan support");
        return false;
    }

    // retrieve all physical devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // pick the first physical device that supports the required features
    vkPhysicalDevice_ = PickPhysicalDevice(devices);
    return true;
}

const VkPhysicalDevice& VulkanDeviceHelper::PickPhysicalDevice(const std::vector<VkPhysicalDevice>& physical_devices)
{
    VkPhysicalDeviceProperties support_properties;
    VkPhysicalDeviceFeatures support_features;
    int cnt = -1, index = -1;
    for (const auto& physical_device : physical_devices)
    {
        index++;

        // get the physical device properties and features
        vkGetPhysicalDeviceProperties(physical_device, &support_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &support_features);

        // log the physical device properties and features
        Logger::LogInfo("-------------------------");
        Logger::LogInfo("Physical Device: " + std::string(support_properties.deviceName));
        // Extract Vulkan version components (major.minor.patch)
        uint32_t apiVersion = support_properties.apiVersion;
        uint32_t major = VK_VERSION_MAJOR(apiVersion);
        uint32_t minor = VK_VERSION_MINOR(apiVersion);
        uint32_t patch = VK_VERSION_PATCH(apiVersion);
        Logger::LogInfo("Vulkan API Version: " + std::to_string(major) + "." + 
                        std::to_string(minor) + "." + std::to_string(patch));


        // count the number of supported properties or features
        cnt = std::max(cnt, CountSupportedPropertiesOrFeatures(device_config_, physical_device));

        // early return if all required properties or features are supported
        if (cnt == device_config_.physical_device_features.size())
        {
            break;
        }
    }

    return physical_devices[index];
}

uint32_t VulkanDeviceHelper::PickQueueFamily(const VkPhysicalDevice& physical_device)
{
    return 0;
}

///-------------------------------------------------------------------------------------------------

// helper function:
// count the number of supported properties or features
int CountSupportedPropertiesOrFeatures(const SVulkanDeviceConfig& device_config, const VkPhysicalDevice& physical_device)
{
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    // Early return if mandatory requirements aren't met
    if ((device_config.physical_device_type && 
        properties.deviceType != device_config.physical_device_type) ||
        (device_config.physical_device_api_version && 
        properties.apiVersion < VK_MAKE_API_VERSION(device_config.physical_device_api_version[0], 
                                                    device_config.physical_device_api_version[1], 
                                                    device_config.physical_device_api_version[2], 
                                                    device_config.physical_device_api_version[3])))
    {
        return -1;
    }

    // Count supported features
    int supported_features = 0;
    for (const auto& feature : device_config.physical_device_features)
    {
        if (feature == VK_TRUE)
        {
            supported_features++;
        }
    }
    
    return supported_features;
}
