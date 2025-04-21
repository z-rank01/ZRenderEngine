#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <map>

#include "vulkan_shader.h"
#include "initialization/vulkan_window.h"
#include "utility/logger.h"

struct SVulkanPipelineConfig
{
    const SVulkanSwapChainConfig* swap_chain_config;
    std::map<EShaderType, const VkShaderModule*> shader_module_map;
    const VkRenderPass* renderpass;
};

class VulkanPipelineHelper
{
private:
    SVulkanPipelineConfig config_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
    const VkDevice* device_;
public:
    VulkanPipelineHelper(SVulkanPipelineConfig config) : config_(config) {}
    ~VulkanPipelineHelper();

    bool CreatePipeline(const VkDevice* device);
};