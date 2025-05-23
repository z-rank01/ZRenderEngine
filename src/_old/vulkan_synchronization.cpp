#include "vulkan_synchronization.h"

VulkanSynchronizationHelper::~VulkanSynchronizationHelper()
{
    for (auto& semaphore : semaphores_)
    {
        vkDestroySemaphore(device_, semaphore.second, nullptr);
    }
    semaphores_.clear();

    for (auto& fence : fences_)
    {
        vkDestroyFence(device_, fence.second, nullptr);
    }
    fences_.clear();
}

bool VulkanSynchronizationHelper::CreateVkSemaphore(std::string id)
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;

    if (semaphores_.find(id) != semaphores_.end())
    {
        Logger::LogError("Semaphore with ID " + id + " already exists.");
        return semaphores_[id];
    }
    VkSemaphore semaphore;
    if (!Logger::LogWithVkResult(
        vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore),
        "Failed to create semaphore " + id,
        "Succeeded in creating semaphore " + id))
    {
        return false;
    }
    semaphores_[id] = semaphore;
    return true;
}

bool VulkanSynchronizationHelper::CreateFence(std::string id)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (fences_.find(id) != fences_.end())
    {
        Logger::LogError("Fence with ID " + id + " already exists.");
        return fences_[id];
    }
    VkFence fence;
    if (!Logger::LogWithVkResult(
        vkCreateFence(device_, &fence_info, nullptr, &fence),
        "Failed to create fence " + id,
        "Succeeded in creating fence " + id))
    {
        return false;
    }
    fences_[id] = fence;
    return true;
}

bool VulkanSynchronizationHelper::WaitForFence(std::string id)
{
    return Logger::LogWithVkResult(vkWaitForFences(device_, 1, &fences_[id], VK_TRUE, UINT64_MAX), 
        "Failed to wait for fence " + id,
        "Succeeded in waiting for fence " + id);
}

bool VulkanSynchronizationHelper::ResetFence(std::string id)
{
    return Logger::LogWithVkResult(vkResetFences(device_, 1, &fences_[id]),
        "Failed to reset fence " + id,
        "Succeeded in resetting fence " + id);
}

VkSemaphore VulkanSynchronizationHelper::GetSemaphore(std::string id) const
{
    if (semaphores_.find(id) != semaphores_.end())
    {
        return semaphores_.at(id);
    }
    return VK_NULL_HANDLE;
}

VkFence VulkanSynchronizationHelper::GetFence(std::string id) const
{
    if (fences_.find(id) != fences_.end())
    {
        return fences_.at(id);
    }
    return VK_NULL_HANDLE;
}
