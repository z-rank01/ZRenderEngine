#pragma once

#include "vulkan_queue.h"
#include <utility/logger.h>


VulkanQueueHelper::VulkanQueueHelper(SVulkanQueueConfig config) : queue_config_(config)
{
    vkQueue_ = VK_NULL_HANDLE;
}

VulkanQueueHelper::~VulkanQueueHelper()
{
}

const VkQueue& VulkanQueueHelper::GetQueueFromDevice(const VkDevice& logical_device)
{
    if (queue_family_index_.has_value())
    {
        queue_index_ = 0; // default to 0 for now
        vkGetDeviceQueue(logical_device, queue_family_index_.value(), queue_index_.value(), &vkQueue_);
    }
    else
    {
        Logger::LogError("Queue family index is not set");
    }
    return vkQueue_;
}

bool VulkanQueueHelper::GenerateQueueCreateInfo(VkDeviceQueueCreateInfo& queue_create_info) const
{
    // check if queue family index is set
    if (!queue_family_index_.has_value()) 
    {
        Logger::LogError("Queue family index is not set");
        return false;
    }

    // create queue
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family_index_.value();
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = new float[1]{ 1.0f };
    return true;
}

void VulkanQueueHelper::PickQueueFamily(const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilyProperties[i].queueFlags & queue_config_.queue_flags)
        {
            // keep the graphic and present queue family index the same
            // so that we only need one family index for both rather than two for each
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentSupport);
            if (presentSupport)
            {
                queue_family_index_ = i;
                Logger::LogInfo("Queue family for both present and graphic index: " + std::to_string(i));
                break;
            }
        }
    }
}

