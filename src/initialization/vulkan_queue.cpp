#pragma once

#include "vulkan_queue.h"
#include <utility/logger.h>

VulkanQueueHelper::VulkanQueueHelper()
{
}

VulkanQueueHelper::VulkanQueueHelper(SVulkanQueueConfig config) : queue_config_(config)
{
}

VulkanQueueHelper::~VulkanQueueHelper()
{
    // vkDestroyQueue(vkQueue_, nullptr);
}

const VkQueue& VulkanQueueHelper::GetQueueFromDevice(const VkDevice& logical_device)
{
    vkGetDeviceQueue(logical_device, queue_family_index_.value(), queue_index_.value(), &vkQueue_);
    return vkQueue_;
}

bool VulkanQueueHelper::GenerateQueueCreateInfo(const VkDeviceQueueCreateInfo& queue_create_info) const
{
    // check if queue family index is set
    if (!queue_family_index_.has_value()) 
    {
        Logger::LogError("Queue family index is not set");
        return false;
    }

    // create queue
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queue_family_index_.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = new float[1]{ 1.0f };
    return true;
}

void VulkanQueueHelper::PickQueueFamily(const VkPhysicalDevice& physical_device)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilyProperties[i].queueFlags & queue_config_.queue_flags)
        {
            queue_family_index_ = i;
        }
    }
}

