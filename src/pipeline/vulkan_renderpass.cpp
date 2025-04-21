#include "vulkan_renderpass.h"

VulkanRenderpassHelper::~VulkanRenderpassHelper()
{
    if (renderpass_ != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(*device_, renderpass_, nullptr);
        renderpass_ = VK_NULL_HANDLE;
    }
}

bool VulkanRenderpassHelper::CreateRenderpass(const VkDevice* device)
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

    // depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = config_.depth_format;
    depthAttachment.samples = config_.sample_count;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // color attachment
    VkAttachmentReference colorReference{};
    colorReference.attachment = 0; // index of the color attachment in the attachment descriptions
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;


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
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = nullptr;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.flags = 0;

    // create render pass
    return Logger::LogWithVkResult(vkCreateRenderPass(*device_, &renderPassInfo, nullptr, &renderpass_), 
        "Failed to create render pass", 
        "Succeeded in creating render pass");
}
