#include "vulkan_renderpass.h"

VulkanRenderpassHelper::~VulkanRenderpassHelper()
{
    if (renderpass_ != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device_, renderpass_, nullptr);
        renderpass_ = VK_NULL_HANDLE;
    }
}

bool VulkanRenderpassHelper::CreateRenderpass(VkDevice device)
{
    device_ = device;

    // color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = config_.color_format;
    colorAttachment.samples = config_.sample_count;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // color attachment reference
    VkAttachmentReference colorReference{};
    colorReference.attachment = 0; // index of the color attachment in the attachment descriptions
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    // subpass dependencies
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments {
        colorAttachment,
    };
    
    // render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.flags = 0;

    // create render pass
    return Logger::LogWithVkResult(vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderpass_), 
        "Failed to create render pass", 
        "Succeeded in creating render pass");
}
