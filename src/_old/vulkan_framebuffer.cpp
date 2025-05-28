#include "vulkan_framebuffer.h"
#include <array>

VulkanFrameBufferHelper::~VulkanFrameBufferHelper()
{
    if (!framebuffers_.empty())
    {
        for (auto framebuffer : framebuffers_)
        {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        framebuffers_.clear();
    }
}

bool VulkanFrameBufferHelper::CreateFrameBuffer(VkRenderPass renderpass)
{
    // 创建帧缓冲
    framebuffers_.resize(config_.swapchain_image_views_.size());
    
    for (size_t i = 0; i < config_.swapchain_image_views_.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            config_.swapchain_image_views_[i],
            config_.depth_image_view_  // 使用从配置中传入的深度图像视图
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = renderpass;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = config_.extent_.width;
        framebuffer_info.height = config_.extent_.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[i]) != VK_SUCCESS)
        {
            Logger::LogError("Failed to create framebuffer");
            return false;
        }
    }

    return true;
}