#pragma once

#include <vulkan/vulkan.h>
#include <optional>

struct SVulkanQueueConfig
{
    VkQueueFlags queue_flags;
};

class VulkanQueueHelper
{
public:
    VulkanQueueHelper() = delete;
    VulkanQueueHelper(SVulkanQueueConfig config);
    ~VulkanQueueHelper();

    const VkQueue& GetQueueFromDevice(const VkDevice& logical_device);
    const VkQueue& GetQueue() const { return vkQueue_; }
    bool GenerateQueueCreateInfo(VkDeviceQueueCreateInfo& queue_create_info) const;
    void PickQueueFamily(const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface);

private:
    SVulkanQueueConfig queue_config_;
    std::optional<uint32_t> queue_family_index_;
    std::optional<uint32_t> queue_index_;

    VkQueue vkQueue_;
};