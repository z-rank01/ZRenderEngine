#include "vulkan_renderpass.h"
#include <array>

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

    // Color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = config_.color_format;
    color_attachment.samples = config_.sample_count;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // depth attachment
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = config_.depth_format;
    depth_attachment.samples = config_.sample_count;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 清除深度缓冲区
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 渲染完成后不需要保留
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 颜色附件引用
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 深度附件引用
    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 子通道描述
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref; // 添加深度附件引用

    // 子通道依赖
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 组合附件
    std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
    
    // 创建渲染通道
    VkRenderPassCreateInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderpass_info.pAttachments = attachments.data();
    renderpass_info.subpassCount = 1;
    renderpass_info.pSubpasses = &subpass;
    renderpass_info.dependencyCount = 1;
    renderpass_info.pDependencies = &dependency;
    renderpass_info.pNext = nullptr;
    renderpass_info.flags = 0;

    // create render pass
    return Logger::LogWithVkResult(vkCreateRenderPass(device_, &renderpass_info, nullptr, &renderpass_),
        "Failed to create render pass",
        "Succeeded in creating render pass");
}
